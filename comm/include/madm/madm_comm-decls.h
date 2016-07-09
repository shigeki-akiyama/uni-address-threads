#ifndef MADM_COMM_TYPES_H
#define MADM_COMM_TYPES_H

#include "madm_misc.h"
#include <cstddef>

namespace madi {
namespace comm {

    typedef size_t pid_t;

    bool initialize(int &argc, char **&argv);
    void finalize();

    template <class... Args>
    void start(void (*f)(int, char **, Args... args),
               int& argc, char **&argv, Args... args);

    void exit(int exitcode) MADI_NORETURN;
    void abort(int exitcode) MADI_NORETURN;

    bool initialized();

    pid_t get_pid();
    size_t get_n_procs();

    template <class T>
    T ** coll_rma_malloc(size_t size);
    template <class T>
    void coll_rma_free(T **ptrs);

    template <class T>
    T * rma_malloc(size_t size);
    template <class T>
    void rma_free(T *p);

    void put(void *dst, void *src, size_t size, pid_t target);
    template <class T>
    void put_value(T *dst, T value, pid_t target);

    void get(void *dst, void *src, size_t size, pid_t target);
    template <class T>
    T get_value(T *src, pid_t target);

    void put_nbi(void *dst, void *src, size_t size, pid_t target);
    void get_nbi(void *dst, void *src, size_t size, pid_t target);

    template <class T>
    T fetch_and_add(T *dst, T value, pid_t target);

    void fence();
    void poll();

    int reg_coll_mmap(void *p, size_t size);
    void reg_coll_munmap(int id);

    void reg_put(int id, void *dst, void *src, size_t size, pid_t target);
    void reg_get(int id, void *dst, void *src, size_t size, pid_t target);

    void barrier();
    bool barrier_try();

    template <class T>
    void broadcast(T *dst, const T &src, pid_t root);

    enum reduce_op {
        reduce_op_sum,
        reduce_op_product,
        reduce_op_min,
        reduce_op_max,
    };

    template <class T>
    void reduce(T dst[], const T src[], size_t size, pid_t root, 
                reduce_op op);

    void native_barrier();

    struct aminfo;

    void amrequest(int tag, void *p, size_t size, int target);
    void amreply(int tag, void *p, size_t size, aminfo *info);

    size_t get_server_mod();

}
}

#endif
