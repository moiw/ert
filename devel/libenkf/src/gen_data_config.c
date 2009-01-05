#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <util.h>
#include <config.h>
#include <ecl_util.h>
#include <enkf_macros.h>
#include <gen_data_config.h>
#include <enkf_types.h>
#include <pthread.h>
#include <path_fmt.h>
#include <gen_data_active.h>
#include <active_list.h>

#define GEN_DATA_CONFIG_ID 90051
struct gen_data_config_struct {
  CONFIG_STD_FIELDS;
  ecl_type_enum  	         internal_type;         /* The underlying type (float | double) of the data in the corresponding gen_data instances. */
  char           	       * template_buffer;       /* Buffer containing the content of the template - read and internalized at boot time. */
  int            	         template_data_offset;  /* The offset into the template buffer before the data should come. */
  int            	         template_data_skip;    /* The length of data identifier in the template.*/ 
  int            	         template_buffer_size;  /* The total size (bytes) of the template buffer .*/
  path_fmt_type  	       * init_file_fmt;         /* file format for the file used to load the inital values - NULL if the instance is initialized from the forward model. */
  gen_data_file_format_type    	 input_format;          /* The format used for loading gen_data instances when the forward model has completed *AND* for loading the initial files.*/
  gen_data_file_format_type    	 output_format;         /* The format used when gen_data instances are written to disk for the forward model. */
  active_list_type             * active_list;           /* List of (EnKF) active indices. */
  pthread_mutex_t                update_lock;           /* mutex serializing (write) access to the gen_data_config object. */
  int                            __report_step;         /* Internal variable used for run_time checking that all instances have the same size (at the same report_step). */
};

/*****************************************************************/

SAFE_CAST(gen_data_config , GEN_DATA_CONFIG_ID)

gen_data_file_format_type gen_data_config_get_input_format ( const gen_data_config_type * config) { return config->input_format; }
gen_data_file_format_type gen_data_config_get_output_format( const gen_data_config_type * config) { return config->output_format; }

int gen_data_config_get_data_size(const gen_data_config_type * config)   { return config->data_size;     }
int gen_data_config_get_report_step(const gen_data_config_type * config) { return config->__report_step; }


int gen_data_config_get_byte_size(const gen_data_config_type * config) {
  return config->data_size * ecl_util_get_sizeof_ctype(config->internal_type);
}

ecl_type_enum gen_data_config_get_internal_type(const gen_data_config_type * config) {
  return config->internal_type;
}


static gen_data_config_type * gen_data_config_alloc__( ecl_type_enum internal_type       , 
						       gen_data_file_format_type input_format ,
						       gen_data_file_format_type output_format,
						       const char * init_file_fmt     ,  
						       const char * template_ecl_file , 
						       const char * template_data_key ) {

  gen_data_config_type * config = util_malloc(sizeof * config , __func__);
  config->__type_id         = GEN_DATA_CONFIG_ID;
  config->data_size  	    = 0;
  config->internal_type     = internal_type;
  config->active_list       = active_list_alloc(0);
  config->input_format      = input_format;
  config->output_format     = output_format;
  config->__report_step     = -1;
  
  if (config->output_format == ASCII_template) {
    if (template_ecl_file == NULL)
      util_abort("%s: internal error - when using format ASCII_template you must supply a temlate file \n",__func__);
  } else
    if (template_ecl_file != NULL)
      util_abort("%s: internal error have template and format mismatch \n",__func__);
  

  if (template_ecl_file != NULL) {
    char *data_ptr;
    config->template_buffer = util_fread_alloc_file_content( template_ecl_file , NULL , &config->template_buffer_size);
    data_ptr = strstr(config->template_buffer , template_data_key);
    if (data_ptr == NULL) 
      util_abort("%s: template:%s can not be used - could not find data key:%s \n",__func__ , template_ecl_file , template_data_key);
    else {
      config->template_data_offset = data_ptr - config->template_buffer;
      config->template_data_skip   = strlen( template_data_key );
    }
  } else 
    config->template_buffer = NULL;

  if (init_file_fmt != NULL)
    config->init_file_fmt     = path_fmt_alloc_path_fmt( init_file_fmt );
  else
    config->init_file_fmt = NULL;

  pthread_mutex_init( &config->update_lock , NULL );
  return config;
}




/**
   This function takes a string representation of one of the
   gen_data_file_format_type values, and returns the corresponding
   integer value.
   
   Will return gen_data_undefined if the string is not recognized,
   calling scope must check on this return value.
*/


static gen_data_file_format_type __gen_data_config_check_format( const char * format ) {
  gen_data_file_format_type type = gen_data_undefined;

  if (strcmp(format , "ASCII") == 0)
    type = ASCII;
  else if (strcmp(format , "ASCII_TEMPLATE") == 0)
    type = ASCII_template;
  else if (strcmp(format , "BINARY_DOUBLE") == 0)
    type = binary_double;
  else if (strcmp(format , "BINARY_FLOAT") == 0)
    type = binary_float;
  
  if (type == gen_data_undefined)
    util_exit("Sorry: format:\"%s\" not recognized - valid values: ASCII / ASCII_TEMPLATE / BINARY_DOUBLE / BINARY_FLOAT \n", format);
  
  return type;
}

/**
   The valid options are:

   INPUT_FORMAT:(ASCII|ASCII_TEMPLATE|BINARY_DOUBLE|BINARY_FLOAT)
   OUTPUT_FORMAT:(ASCII|ASCII_TEMPLATE|BINARY_DOUBLE|BINARY_FLOAT)
   INIT_FILES:/some/path/with/%d
   TEMPLATE:/some/template/file
   KEY:<SomeKeyFoundInTemplate>

*/

gen_data_config_type * gen_data_config_alloc(int num_options, const char ** options) {
  const ecl_type_enum internal_type = ecl_double_type;
  gen_data_config_type * config;
  hash_type * opt_hash = hash_alloc_from_options( num_options , options );

  /* Parsing options */
  {
    gen_data_file_format_type input_format  = gen_data_undefined;
    gen_data_file_format_type output_format = gen_data_no_export;
    char * template_file = NULL;
    char * template_key  = NULL;
    char * init_file_fmt = NULL;

    const char * option = hash_iter_get_first_key( opt_hash );
    while (option != NULL) {
      const char * value = hash_get(opt_hash , option);

      /* 
	 That the various options are internally consistent is ensured
	 in the final static allocater.
      */
      if (strcmp(option , "INPUT_FORMAT") == 0) 
	input_format = __gen_data_config_check_format( value );
      
      else if (strcmp(option , "OUTPUT_FORMAT") == 0)
	output_format = __gen_data_config_check_format( value );

      else if (strcmp(option , "TEMPLATE") == 0)
	template_file = util_alloc_string_copy( value );
      
      else if (strcmp(option , "KEY") == 0)
	template_key = util_alloc_string_copy( value );
      
      else if (strcmp(option , "INIT_FILES") == 0)
	init_file_fmt = util_alloc_string_copy( value );
      
      else
	fprintf(stderr , "%s: Warning: \'%s:%s\' not recognized as valid option - ignored \n",__func__ , option , value);

      
      option = hash_iter_get_next_key( opt_hash );
    } 
    config = gen_data_config_alloc__(internal_type , input_format , output_format , init_file_fmt , template_file , template_key);
    util_safe_free( init_file_fmt );
    util_safe_free( template_file );
    util_safe_free( template_key );
  }
  hash_free(opt_hash);
  return config;
}




void gen_data_config_free(gen_data_config_type * config) {
  active_list_free(config->active_list);
  if (config->init_file_fmt != NULL) path_fmt_free(config->init_file_fmt);
  free(config);
}




/**
   This function gets a size (from a gen_data) instance, and verifies
   that the size agrees with the currently stored size and
   report_step. If the report_step is new we just recordthe new info,
   otherwise it will break hard.
*/


void gen_data_config_assert_size(gen_data_config_type * config , int size, int report_step) {
  pthread_mutex_lock( &config->update_lock );
  {
    if (report_step != config->__report_step) {
      config->data_size = size; 
      config->__report_step = report_step;
    } else 
      if (config->data_size != size)
	util_abort("%s: Size mismatch when loading:%s from file - got %d elements - expected:%d [report_step:%d] \n",__func__ , config->ecl_kw_name , size , config->data_size, report_step);
    active_list_set_data_size( config->active_list , size );
  }
  pthread_mutex_unlock( &config->update_lock );
}





char * gen_data_config_alloc_initfile(const gen_data_config_type * config , int iens) {
  if (config->init_file_fmt != NULL) {
    char * initfile = path_fmt_alloc_path(config->init_file_fmt , false , iens);
    return initfile;
  } else 
    return NULL; 
}




void gen_data_config_get_template_data( const gen_data_config_type * config , 
					char ** template_buffer    , 
					int * template_data_offset , 
					int * template_buffer_size , 
					int * template_data_skip) {
  
  *template_buffer      = config->template_buffer;
  *template_data_offset = config->template_data_offset;
  *template_buffer_size = config->template_buffer_size;
  *template_data_skip   = config->template_data_skip;
  
}




void gen_data_config_activate(gen_data_config_type * config , active_mode_type active_mode , void * active_config) {
  gen_data_active_type * active = gen_data_active_safe_cast( active_config );

  if (active_mode == all_active)
    active_list_set_all_active(config->active_list);
  else {
    active_list_reset(config->active_list);
    if (active_mode == partly_active) 
      gen_data_active_update_active_list( active , config->active_list);
  }
    
}



/*****************************************************************/

VOID_FREE(gen_data_config)
GET_ACTIVE_LIST(gen_data)
VOID_CONFIG_ACTIVATE(gen_data)
