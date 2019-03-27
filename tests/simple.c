#include <stdio.h>
#include <string.h>
#include "drob.h"

static int custom_strlen(const char *str1)
{
    int i = 0;

    while (str1[i] != 0) {
        i++;
    }

    return i;
}

int main(int argc, char **argv)
{
    drob_cfg *cfg;
    drob_f func;
    int ret;

    if (argc != 2) {
        fprintf(stderr, "1 argument required\n");
        return 0;
    }

    if (drob_setup()) {
        fprintf(stderr, "Cannot setup libros\n");
        return 0;
    }
    drob_set_logging(stdout, DROB_LOGLEVEL_DEBUG);

    ret = custom_strlen(argv[1]);
    printf("String length: %d\n", ret);

    cfg = drob_cfg_new1(DROB_PARAM_TYPE_INT, DROB_PARAM_TYPE_PTR);
    drob_cfg_set_param_ptr(cfg, 0, argv[1]);
    drob_cfg_set_ptr_flag(cfg, 0, DROB_PTR_FLAG_CONST);
    drob_cfg_set_ptr_flag(cfg, 0, DROB_PTR_FLAG_NOTNULL);
    drob_cfg_set_ptr_flag(cfg, 1, DROB_PTR_FLAG_RESTRICT);
    drob_cfg_dump(cfg);

    func = drob_optimize(custom_strlen, cfg);
    if (func) {
        ret = ((typeof(custom_strlen)*)func)(argv[1]);
        printf("String length: %d\n", ret);
        drob_free(func);
    }

//    func = drob_optimize(strlen, cfg);
//    if (func) {
//        ret = ((typeof(strlen)*)(func))(argv[1]);
//        printf("String length: %d\n", ret);
//        drob_free(func);
//    }

    drob_cfg_free(cfg);
    drob_teardown();

    return 0;
}
