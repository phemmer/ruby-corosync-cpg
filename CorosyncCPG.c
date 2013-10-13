#include <corosync/cpg.h>
#include "ruby.h"

static void ccpg_free(void *p) {
	cpg_finalize(*(cpg_handle_t *)p);
	free(p);
}

static VALUE ccpg_new(VALUE class) {
	cpg_model_v1_data_t cpg_model_v1_data;
	cpg_handle_t *cpg_handle;

	memset(&cpg_model_v1_data, 0, sizeof(cpg_model_v1_data_t));

	//cpg_model_v1_data.cpg_deliver_fn = &cpg_deliver_fn;
	//cpg_model_v1_data.cpg_confchg_fn = &cpg_confchg_fn;

	cpg_handle = ALLOC(cpg_handle_t);


	cs_error_t rc = cpg_model_initialize(cpg_handle, CPG_MODEL_V1, (cpg_model_data_t *)&cpg_model_v1_data, NULL);
	if (rc != CS_OK) {
		char *hint = "";
		switch (rc) {
			case CS_ERR_LIBRARY:
				hint = " (Is corosync running?)";
				break;
			case CS_ERR_ACCESS:
				hint = " (Are you root?)";
				break;
		}

		rb_raise(rb_const_get(rb_cObject, rb_intern("StandardError")),
				"Could not connect to CPG%s: rc=%d %s", hint, rc, cs_strerror(rc));
	}

	VALUE instance = Data_Wrap_Struct(class, 0, ccpg_free, cpg_handle);

	return instance;
}

VALUE cCorosyncCPG;

void Init_CorosyncCPG() {
	cCorosyncCPG = rb_define_class("CorosyncCPG", rb_cObject);
	//rb_define_singleton_method(cCorosyncCPG, "new", cpg_new, 1);
	rb_define_singleton_method(cCorosyncCPG, "new", ccpg_new, 0);
}
