#include <ecl_grid.h>
#include <ecl_util.h>
#include <fortio.h>
#include <stdbool.h>
#include <ecl_kw.h>
#include <util.h>
#include <string.h>
#include <int_vector.h>
#include <double_vector.h>



void extract_contact(const ecl_kw_type * swat1 , const ecl_kw_type * swat2 , const ecl_grid_type * ecl_grid , const int_vector_type * ilist , const int_vector_type * jlist , double_vector_type * contact) {
  int ii,jj,k,nz,active_size,nx,ny;
  double * diff;
  
  ecl_grid_get_dims( ecl_grid , &nx, &ny , &nz , &active_size);
  diff = util_malloc( nz * sizeof * diff , __func__);
  
  for (ii=0; ii < int_vector_size(ilist); ii++) {
    for (jj=0; jj < int_vector_size(jlist); jj++) {
      int i = int_vector_iget( ilist , ii);
      int j = int_vector_iget( jlist , jj);

      for (k=0; k < nz; k++) {
	int active_index = ecl_grid_get_active_index( ecl_grid , i , j , k);
	if (active_index >= 0) {
	  double sw1 = ecl_kw_iget_as_double( swat1 , active_index );
	  double sw2 = ecl_kw_iget_as_double( swat2 , active_index );

	  diff[k] = (sw2 - sw1) / sw1;
	} else
	  diff[k] = 0;
      }
      {
	char * filename = util_alloc_sprintf("profile_%d.%d" , i ,j);
	FILE * stream   = util_fopen(filename , "w");
	
	for (k=0; k < nz; k++)
	  fprintf(stream , "%d   %g \n",k,diff[k]);
	
	fclose(stream);
	free(filename);
      }
    }
  }
  free( diff );
}





int main (int argc , char ** argv) {
  if (argc != 5) 
    util_exit("%s: needs four arguments: GRID_FILE  RESTART_FILE1  RESTART_FILE2  INDEX_FILE \n");
  
  {
    const char * grid_file     = argv[1];
    const char * restart_file1 = argv[2];
    const char * restart_file2 = argv[3];
    const char * index_file    = argv[4];
    int_vector_type * ilist    = int_vector_alloc(100 , -1);
    int_vector_type * jlist    = int_vector_alloc(100 , -1);
    double_vector_type * contact = double_vector_alloc(100 , 0.0);

    int nx,ny,nz,active_size;
    bool fmt_file;
    ecl_file_type file_type;
    ecl_grid_type * grid = ecl_grid_alloc( grid_file , true );
    ecl_kw_type 	* swat1 = NULL;
    ecl_kw_type 	* swat2 = NULL;
  
  
    ecl_grid_get_dims( grid , &nx , &ny , &nz , &active_size );
    ecl_util_get_file_type( restart_file1 , &file_type , &fmt_file , NULL);
    if (file_type != ecl_restart_file)
      util_exit("Files must be of type not unified restart_file \n");
    {
      fortio_type * fortio = fortio_fopen(restart_file1 , "r" , true , fmt_file);
      if (ecl_kw_fseek_kw("SWAT" , true , false , fortio))
	swat1 = ecl_kw_fread_alloc(fortio);
      fortio_fclose( fortio );
    }

    {
      fortio_type * fortio = fortio_fopen(restart_file2 , "r" , true , fmt_file);
      if (ecl_kw_fseek_kw("SWAT" , true , false , fortio))
	swat2 = ecl_kw_fread_alloc(fortio);
      fortio_fclose( fortio );
    }

    {
      FILE * stream = util_fopen(index_file , "r");
      int read_count;
      do {
	int i,j;
	read_count = fscanf(stream , "%d %d" , &i , &j);
	if (read_count == 2) {
	  int_vector_append(ilist , i);
	  int_vector_append(jlist , j);
	}
      } while (read_count == 2);
      fclose(stream);
    }

    extract_contact(swat1 , swat2 , grid , ilist , jlist , contact);
    ecl_kw_free(swat1);
    ecl_kw_free(swat2);
    ecl_grid_free( grid );
  }
}
