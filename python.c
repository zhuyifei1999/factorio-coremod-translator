/* SPDX-License-Identifier: GPL-2.0 */

#include <Python.h>

#include <dlfcn.h>
#include <error.h>
#include <errno.h>

#include "translator.h"

static PyObject *
factoriotranslatorc_getfd(PyObject *self, PyObject *args)
{
	return PyLong_FromLong(rpc_channel_sockets[1]);
}

static PyObject *
factoriotranslatorc_apply_translation_to_console(PyObject *self, PyObject *args)
{
	const char *orig_str, *new_str;
	Py_ssize_t orig_len, new_len;

	if (!PyArg_ParseTuple(args, "s#s#:apply_translation_to_console",
			      &orig_str, &orig_len,
			      &new_str, &new_len))
		return NULL;

	apply_translation_to_console(orig_str, orig_len, new_str, new_len);
	Py_RETURN_NONE;
}

static PyObject *
factoriotranslatorc_apply_translation_to_textbox(PyObject *self, PyObject *args)
{
	const char *src_str, *dest_str, *explain_str;
	Py_ssize_t src_len, dest_len, explain_len;

	if (!PyArg_ParseTuple(args, "s#s#s#:apply_apply_translation_to_textbox",
			      &src_str, &src_len,
			      &dest_str, &dest_len,
			      &explain_str, &explain_len))
		return NULL;

	apply_translation_to_textbox(src_str, src_len, dest_str, dest_len, explain_str, explain_len);
	Py_RETURN_NONE;
}

static PyObject *
factoriotranslatorc_textbox_translate_error(PyObject *self, PyObject *args)
{
	textbox_translate_error();
	Py_RETURN_NONE;
}

static PyMethodDef factoriotranslatorc_module_methods[] = {
	{"getfd", factoriotranslatorc_getfd, METH_NOARGS},
	{"apply_translation_to_console", factoriotranslatorc_apply_translation_to_console, METH_VARARGS},
	{"apply_translation_to_textbox", factoriotranslatorc_apply_translation_to_textbox, METH_VARARGS},
	{"textbox_translate_error", factoriotranslatorc_textbox_translate_error, METH_NOARGS},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef factoriotranslatorc_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "factoriotranslatorc",
	.m_size = 0,
	.m_methods = factoriotranslatorc_module_methods,
};

static PyObject *
PyInit_factoriotranslatorc(void)
{
	return PyModuleDef_Init(&factoriotranslatorc_module);
}

static const char *getselfpath(void)
{
	Dl_info info = {0};

	dladdr(&getselfpath, &info);

	if (!info.dli_fname)
		error(1, 0, "libfactoriotranslator.so: could not resolve my path");

	return info.dli_fname;
}

#if 0
static wchar_t *convert_mbs(const char *src) {
	const char *p = src;
	wchar_t *dest;
	size_t n;

	n = mbsrtowcs(NULL, &p, 0, NULL);
	if (n < 0)
		error(1, errno, "mbsrtowcs");

	dest = calloc(n + 1, sizeof(wchar_t));
	if (!dest)
		error(1, errno, "malloc");

	/* Can't fail because it only fails on invalid multibyte sequence,
	 * which has been check already */
	p = src;
	assert(mbsrtowcs(dest, &p, n + 1, NULL) >= 0);

	return dest;
}
#endif

static void *python_thread(void *ignored)
{
	PyObject *sys_path, *selfpath = NULL, *mainmod = NULL;
	PyStatus status;
	PyConfig config;

	PyConfig_InitIsolatedConfig(&config);

	PyImport_AppendInittab("factoriotranslatorc", &PyInit_factoriotranslatorc);

/* status is an aggregate */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waggregate-return"

#if 0
	/* This removes default paths, can't use. */
	status = PyWideStringList_Insert(&config.module_search_paths,
					 0, convert_mbs(getselfpath()));
	if (PyStatus_Exception(status))
		goto exception;
	config.module_search_paths_set = 1;
	config.site_import = 0;
#endif

	status = Py_InitializeFromConfig(&config);
	if (PyStatus_Exception(status))
		goto exception;
	PyConfig_Clear(&config);

#pragma GCC diagnostic pop

	sys_path = PySys_GetObject("path");
	if (!sys_path) {
		PyErr_SetString(PyExc_RuntimeError, "Can't get sys.path");
		goto out;
	}

	selfpath = PyUnicode_FromString(getselfpath());
	if (!selfpath)
		goto out;

	if (PyList_Insert(sys_path, 0, selfpath))
		goto out;

	mainmod = PyImport_ImportModule("factoriotranslator");

out:
	if (PyErr_Occurred())
		PyErr_Print();
	else
		PySys_WriteStderr("Translator Python exited\n");

	Py_XDECREF(mainmod);
	Py_XDECREF(selfpath);

	Py_Finalize();

	/* We should be running indefinitely */
	exit(1);

exception:
	PyConfig_Clear(&config);
	Py_ExitStatusException(status);
}

void start_python(void)
{
	pthread_t thread;

	if (pthread_create(&thread, NULL, python_thread, NULL))
		error(1, errno, "pthread_create");
}
