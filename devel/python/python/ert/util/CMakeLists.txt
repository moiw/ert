add_python_package("Python ert.util" ${PYTHON_INSTALL_PREFIX}/ert/util 
                   "ctime.py;hash.py;__init__.py;latex.py;libutil.py;buffer.py;lookup_table.py;matrix.py;stat.py;stringlist.py;tvector.py;util_func.py;log.py" T)

add_python_package("Python ert.util test" ${PYTHON_INSTALL_PREFIX}/ert/util "libtest_util.py;test_area.py" F)
