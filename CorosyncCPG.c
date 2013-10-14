#include <corosync/cpg.h>
#include <pthread.h>
#include "ruby.h"

typedef struct _ccpg_instance {
	cpg_handle_t handle;
	VALUE object;
	struct _ccpg_instance *next;
} ccpg_instance;

pthread_rwlock_t ccpg_instance_lock = PTHREAD_RWLOCK_INITIALIZER;
ccpg_instance *ccpg_instance_first = NULL;
ccpg_instance *ccpg_instance_last = NULL;

ccpg_instance *ccpg_instance_add(ccpg_instance *instance) {
	pthread_rwlock_wrlock(&ccpg_instance_lock);

	if (ccpg_instance_first == NULL) {
		ccpg_instance_first = ccpg_instance_last = instance;
	}
	instance->next = NULL;

	pthread_rwlock_unlock(&ccpg_instance_lock);

	return instance;
}
void ccpg_instance_delete(ccpg_instance *instance) {
	ccpg_instance *node, *node_prev;

	pthread_rwlock_wrlock(&ccpg_instance_lock);

	for (node = ccpg_instance_first, node_prev = NULL;
			node != NULL;
			node = (node_prev = node)->next) {
		if (node == instance) {
			if (node_prev != NULL) {
				// we're not the first node
				node_prev->next = instance->next;
			} else {
				// we're the first node
				ccpg_instance_first = instance->next;
			}
			if (ccpg_instance_last == instance) {
				// we're the last node
				ccpg_instance_last = node_prev;
			}
			pthread_rwlock_unlock(&ccpg_instance_lock);
			return;
		}
	}

	// if we got here then the target wasn't in the list...
	pthread_rwlock_unlock(&ccpg_instance_lock);
	return;
}
ccpg_instance *ccpg_instance_find_by_handle(cpg_handle_t handle) {
	ccpg_instance *node;

	pthread_rwlock_rdlock(&ccpg_instance_lock);

	for (node = ccpg_instance_first;
			node != NULL;
			node = node->next) {
		if (node->handle == handle) {
			pthread_rwlock_unlock(&ccpg_instance_lock);
			return node;
		}
	}
	pthread_rwlock_unlock(&ccpg_instance_lock);
	return NULL;
}



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
	ccpg_instance *instance;
	Data_Get_Struct(self, ccpg_instance, instance);

	struct cpg_name cpg_name;
	char *group_name_str = RSTRING_PTR(group);
	int group_name_len = RSTRING_LEN(group);
	if (group_name_len > sizeof(cpg_name.value)) {
		rb_raise(rb_eArgError, "Group name must be %lu characters or less", sizeof(cpg_name.value));
		return Qnil;
	}
	strncpy(cpg_name.value, group_name_str, group_name_len);
	cpg_name.length = group_name_len;

	cs_error_t rc = cpg_join(instance->handle, &cpg_name);
	if (rc != CS_OK) {
		rb_raise(rb_eStandardError, "%s", cs_function_error_str(CPG_JOIN, rc));
		return Qnil;
	}

	return Qnil;
}

VALUE ccpg_mcast_joined(VALUE self, VALUE message_list) {
	ccpg_instance *instance;
	Data_Get_Struct(self, ccpg_instance, instance);

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

	cs_error_t rc = cpg_mcast_joined(instance->handle, CPG_TYPE_AGREED, iovec_list, iovec_len);
	free(iovec_list);
	if (rc != CS_OK) {
		rb_raise(rb_eStandardError, "%s", cs_function_error_str(CPG_JOIN, rc));
		return Qnil;
	}

	return Qnil;
}

static void ccpg_free(ccpg_instance *instance) {
	cpg_finalize(instance->handle);
	ccpg_instance_delete(instance);
	free(instance);
}

static VALUE ccpg_new(VALUE class) {
	cpg_model_v1_data_t cpg_model_v1_data;
	ccpg_instance *instance;

	memset(&cpg_model_v1_data, 0, sizeof(cpg_model_v1_data_t));

	//cpg_model_v1_data.cpg_deliver_fn = &cpg_deliver_fn;
	//cpg_model_v1_data.cpg_confchg_fn = &cpg_confchg_fn;

	instance = ALLOC(ccpg_instance);
	memset(instance, 0, sizeof(ccpg_instance));
	ccpg_instance_add(instance);

	cs_error_t rc = cpg_model_initialize(&instance->handle, CPG_MODEL_V1, (cpg_model_data_t *)&cpg_model_v1_data, NULL);
	if (rc != CS_OK) {
		ccpg_free(instance);
		rb_raise(rb_eStandardError, "Could not connect to CPG: rc=%d; %s",
			rc, cs_function_error_str(CPG_MODEL_INITIALIZE, rc));
		return Qnil;
	}

	VALUE object = Data_Wrap_Struct(class, 0, ccpg_free, instance);
	instance->object = object;

	return object;
}

VALUE cCorosyncCPG;

void Init_CorosyncCPG() {

	cCorosyncCPG = rb_define_class("CorosyncCPG", rb_cObject);
	//rb_define_singleton_method(cCorosyncCPG, "new", cpg_new, 1);
	rb_define_singleton_method(cCorosyncCPG, "new", ccpg_new, 0);
	rb_define_method(cCorosyncCPG, "join", ccpg_join, 1);
	rb_define_method(cCorosyncCPG, "mcast_joined", ccpg_mcast_joined, 1);
}
