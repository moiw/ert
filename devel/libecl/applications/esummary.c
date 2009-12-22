#include <ecl_kw.h>
#include <stdlib.h>
#include <ecl_sum.h>
#include <util.h>
#include <string.h>
#include <signal.h>
#include <vector.h>


void install_SIGNALS(void) {
  signal(SIGSEGV , util_abort_signal);    /* Segmentation violation, i.e. overwriting memory ... */
  signal(SIGINT  , util_abort_signal);    /* Control C */
  signal(SIGTERM , util_abort_signal);    /* If killing the enkf program with SIGTERM (the default kill signal) you will get a backtrace. Killing with SIGKILL (-9) will not give a backtrace.*/
}

void usage() {
  fprintf(stderr," The esummary.x program can be used to extract summary vectors from \n");
  fprintf(stderr," an ensemble of summary files: \n\n");
  fprintf(stderr,"    bash%% esummary.x  ECLIPSE1.DATA ECLIPSE2.DATA  KEY1  KEY2  ... \n\n"); 
  exit(1);
}




int main(int argc , char ** argv) {
  install_SIGNALS();
  {
    if (argc < 3) usage(); 

    {
      ecl_sum_type * first_ecl_sum;   /* This governs the timing */
      vector_type  * ecl_sum_list = vector_alloc_new();
      int nvars;
      char ** var_list;
      bool *  has_var;

      /** Loading the data */
      {
        int iarg     = 1;
        /*  Some sorting here???  */
        while (iarg < argc && util_file_exists( argv[iarg] )) {
          char * path , * basename;
          ecl_sum_type * ecl_sum;
          util_alloc_file_components( argv[iarg] , &path , &basename  , NULL); 
          ecl_sum = ecl_sum_fread_alloc_case( argv[iarg] , ":");
          if (iarg == 1)
            first_ecl_sum = ecl_sum;  /* Keep track of this  - might sort the vector */

          fprintf(stderr,"Loading case: %s/%s" , path , basename); fflush(stderr);
          vector_append_owned_ref( ecl_sum_list , ecl_sum , ecl_sum_free__ );
          iarg++;
          fprintf(stderr,"\n");
          util_safe_free( path );
          free( basename );
        }
      }
      nvars    =  argc - vector_get_size( ecl_sum_list ) - 1;
      if (nvars == 0) util_exit(" --- No variables \n");
      var_list = &argv[vector_get_size( ecl_sum_list ) + 1];
      has_var = util_malloc( nvars * sizeof * has_var , __func__);

      
      /** Checking time consistency - and discarding those with unmatching time vector. */
      {
        int i;
        time_t_vector_type * first_time = ecl_sum_alloc_time_vector( vector_iget_const( ecl_sum_list , 0) , true );

        for (i=1; i < vector_get_size( ecl_sum_list); i++) {
          time_t_vector_type * time_vector;
          const  ecl_sum_type * ecl_sum = vector_iget_const( ecl_sum_list , i );
          if (ecl_sum_get_first_report_step( ecl_sum ) >= 0 ) {
            time_vector = ecl_sum_alloc_time_vector( ecl_sum , true );
            int i;
            for (i=0; i < util_int_min( time_t_vector_size( first_time )  , time_t_vector_size( time_vector )); i++) {
              if (time_t_vector_iget( first_time , i) != time_t_vector_iget(time_vector , i)) {
                vector_iset_ref( ecl_sum_list , i , NULL);
                printf("Discarding case:%s due to time inconsistencies \n" , ecl_sum_get_case( ecl_sum ));
                break;
              }
            }
            time_t_vector_free( time_vector );
          } else {
            vector_iset_ref( ecl_sum_list , i , NULL);
            printf("Discarding case:%s - no data \n" , ecl_sum_get_case( ecl_sum ));
          }
        }
        time_t_vector_free( first_time );
      }
      
      
      /* 
         Checking that the summary files have the various variables -
         if a variable is missing from one of the cases it is
         completely discarded.
      */
      {
        int iens,ivar;
        
        /* Checking the variables */
        for (ivar = 0; ivar < nvars; ivar++) 
          has_var[ivar] = true;
        
        for (iens = 0; iens < vector_get_size( ecl_sum_list ); iens++) {
          const ecl_sum_type * ecl_sum = vector_iget_const( ecl_sum_list , iens );
          for (ivar = 0; ivar < nvars; ivar++) {
            if (has_var[ivar]) {
              if (!ecl_sum_has_general_var( ecl_sum , var_list[ivar] )) {
                fprintf(stderr,"** Warning: could not find variable: \'%s\' in case:%s - completely discarded.\n", var_list[ivar] , ecl_sum_get_case(ecl_sum));
                has_var[ivar] = false;
              }
            }
          }
        }
      }


      /** The actual summary lookup. */
      {
        int first_report = ecl_sum_get_first_report_step( first_ecl_sum );
        int last_report  = ecl_sum_get_last_report_step( first_ecl_sum );
        FILE * stream = stdout;
        int iens,ivar,report;
        
        for (report = first_report; report <= last_report; report++) {
          for (ivar = 0; ivar < nvars; ivar++) {                                          /* Iterating over the variables */
            if (has_var[ivar]) { 
              for (iens = 0; iens < vector_get_size( ecl_sum_list ); iens++) {            /* Iterating over the ensemble members */
                const ecl_sum_type * ecl_sum = vector_iget_const( ecl_sum_list , iens );  
                double value = 0;
                int ministep1 , ministep2;
                if (ecl_sum_has_report_step(ecl_sum , report)) {
                  ecl_sum_report2ministep_range( ecl_sum , report , &ministep1 , &ministep2);
                  ministep1 = ministep2;
                  if (ecl_sum_has_ministep(ecl_sum , ministep2)) {
                    if (ivar == 0 && iens == 0) {                                         /* Display time info in the first columns */
                      int day,month,year;
                      util_set_date_values(ecl_sum_get_sim_time(ecl_sum , ministep2) , &day , &month, &year);
                      fprintf(stream , "%7.2f   %02d/%02d/%04d   " , ecl_sum_get_sim_days(ecl_sum , ministep2) , day , month , year);
                    }
                    value = ecl_sum_get_general_var(ecl_sum , ministep2 , var_list[ivar]);
                  }
                }
                fprintf(stream , " %12.3f " , value);
              }
            }
          }
          fprintf(stream , "\n");
        }
      }
      vector_free( ecl_sum_list );
      free( has_var );
    }
  }
}
