#include <corosync/cpg.h>
#include "ruby.h"

enum cs_functions {
	CPG_MODEL_INITIALIZE,
	CPG_JOIN,
};
const char *cs_function_error_strings[][CS_ERR_SECURITY + 1] = {
	[CPG_MODEL_INITIALIZE] = {
		[CS_ERR_ACCESS] = "Permission denied",
		[CS_ERR_LIBRARY] = "Connection failed",
	},
};
const char *cs_function_error_str(const unsigned int funcnum, cs_error_t err) {
	//TODO make this a macro
	const char *string = cs_function_error_strings[funcnum][err];
	if (string != 0)
		return(string);
	return cs_strerror(err);
}

VALUE ccpg_join(VALUE self, VALUE group) {
	cpg_handle_t *cpg_handle;
	Data_Get_Struct(self, cpg_handle_t, cpg_handle);

	struct cpg_name cpg_name;
	char *group_name_str = RSTRING_PTR(group);
	int group_name_len = RSTRING_LEN(group);
	if (group_name_len > sizeof(cpg_name.value)) {
		rb_raise(rb_eArgError, "Group name must be %lu characters or less", sizeof(cpg_name.value));
		return Qnil;
	}
	strncpy(cpg_name.value, group_name_str, group_name_len);
	cpg_name.length = group_name_len;

	cs_error_t rc = cpg_join(*cpg_handle, &cpg_name);
	if (rc != CS_OK) {
		rb_raise(rb_eStandardError, "%s", cs_function_error_str(CPG_JOIN, rc));
		return Qnil;
	}

	return Qnil;
}

VALUE ccpg_mcast_joined(VALUE self, VALUE message_list) {
	cpg_handle_t *cpg_handle;
	Data_Get_Struct(self, cpg_handle_t, cpg_handle);

	int iovec_len;
	struct iovec *iovec_list;
	if (TYPE(message_list) == T_ARRAY) {
		if (RARRAY_LEN(message_list) > INT_MAX) {
			rb_raise(rb_eArgError, "Can not send more than %d messages", INT_MAX);
			return Qnil;
		}
		iovec_len = RARRAY_LEN(message_list);
		iovec_list = (struct iovec *) ALLOC_N(struct iovec, iovec_len);
		VALUE string;
		long i;
		for (i = 0; i < iovec_len; i++) {
			string = rb_ary_entry(message_list, i);
			iovec_list[i].iov_base = RSTRING_PTR(string);
			iovec_list[i].iov_len = RSTRING_LEN(string);
		}
	} else if (TYPE(message_list) == T_STRING) {
		iovec_len = 1;
		iovec_list = (struct iovec *) ALLOC(struct iovec);
		iovec_list[0].iov_base = RSTRING_PTR(message_list);
		iovec_list[0].iov_len = RSTRING_LEN(message_list);
	} else {
		rb_raise(rb_eArgError, "Unsupported message type. Must be a string or array");
		return Qnil;
	}

	cs_error_t rc = cpg_mcast_joined(*cpg_handle, CPG_TYPE_AGREED, iovec_list, iovec_len);
	free(iovec_list);
	if (rc != CS_OK) {
		rb_raise(rb_eStandardError, "%s", cs_function_error_str(CPG_JOIN, rc));
		return Qnil;
	}

	return Qnil;
}

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
		rb_raise(rb_const_get(rb_cObject, rb_intern("StandardError")),
				"Could not connect to CPG: rc=%d; %s", rc, cs_function_error_str(CPG_MODEL_INITIALIZE, rc));
		return Qnil;
	}

	VALUE instance = Data_Wrap_Struct(class, 0, ccpg_free, cpg_handle);

	return instance;
}

VALUE cCorosyncCPG;

void Init_CorosyncCPG() {
	cCorosyncCPG = rb_define_class("CorosyncCPG", rb_cObject);
	//rb_define_singleton_method(cCorosyncCPG, "new", cpg_new, 1);
	rb_define_singleton_method(cCorosyncCPG, "new", ccpg_new, 0);
	rb_define_method(cCorosyncCPG, "join", ccpg_join, 1);
	rb_define_method(cCorosyncCPG, "mcast_joined", ccpg_mcast_joined, 1);
}
