/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/*my code*/
#define _GNU_SOURCE         /* See feature_test_macros(7) */
/* end my code*/


#include <strings.h>

#include "mpiimpl.h"
#include "mpi_init.h"

/*TIMNDUS CODE_0*/

    #include <stdlib.h>
    #include "stdio.h"
    #include <string.h>
    #include <unistd.h>
    #include <sched.h>
    #include <time.h>

    #define LEN 256

/*END OF CODE_0*/

/*
=== BEGIN_MPI_T_CVAR_INFO_BLOCK ===

categories:
    - name        : THREADS
      description : multi-threading cvars

cvars:
    - name        : MPIR_CVAR_ASYNC_PROGRESS
      category    : THREADS
      type        : boolean
      default     : false
      class       : device
      verbosity   : MPI_T_VERBOSITY_USER_BASIC
      scope       : MPI_T_SCOPE_ALL_EQ
      description : >-
        If set to true, MPICH will initiate an additional thread to
        make asynchronous progress on all communication operations
        including point-to-point, collective, one-sided operations and
        I/O.  Setting this variable will automatically increase the
        thread-safety level to MPI_THREAD_MULTIPLE.  While this
        improves the progress semantics, it might cause a small amount
        of performance overhead for regular MPI operations.  The user
        is encouraged to leave one or more hardware threads vacant in
        order to prevent contention between the application threads
        and the progress thread(s).  The impact of oversubscription is
        highly system dependent but may be substantial in some cases,
        hence this recommendation.

    - name        : MPIR_CVAR_DEFAULT_THREAD_LEVEL
      category    : THREADS
      type        : string
      default     : "MPI_THREAD_SINGLE"
      class       : device
      verbosity   : MPI_T_VERBOSITY_USER_BASIC
      scope       : MPI_T_SCOPE_ALL_EQ
      description : >-
        Sets the default thread level to use when using MPI_INIT. This variable
        is case-insensitive.

=== END_MPI_T_CVAR_INFO_BLOCK ===
*/

/* -- Begin Profiling Symbol Block for routine MPI_Init */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Init = PMPI_Init
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPI_Init  MPI_Init
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Init as PMPI_Init
#elif defined(HAVE_WEAK_ATTRIBUTE)
int MPI_Init(int *argc, char ***argv) __attribute__((weak,alias("PMPI_Init")));
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPI_Init
#define MPI_Init PMPI_Init

/* Fortran logical values. extern'd in mpiimpl.h */
/* MPI_Fint MPIR_F_TRUE, MPIR_F_FALSE; */

/* Any internal routines can go here.  Make them static if possible */

/* must go inside this #ifdef block to prevent duplicate storage on darwin */
int MPIR_async_thread_initialized = 0;
#endif

#undef FUNCNAME
#define FUNCNAME MPI_Init

/*@
   MPI_Init - Initialize the MPI execution environment

Input Parameters:
+  argc - Pointer to the number of arguments 
-  argv - Pointer to the argument vector

Thread and Signal Safety:
This routine must be called by one thread only.  That thread is called
the `main thread` and must be the thread that calls 'MPI_Finalize'.

Notes:
   The MPI standard does not say what a program can do before an 'MPI_INIT' or
   after an 'MPI_FINALIZE'.  In the MPICH implementation, you should do
   as little as possible.  In particular, avoid anything that changes the
   external state of the program, such as opening files, reading standard
   input or writing to standard output.

Notes for C:
    As of MPI-2, 'MPI_Init' will accept NULL as input parameters. Doing so
    will impact the values stored in 'MPI_INFO_ENV'.

Notes for Fortran:
The Fortran binding for 'MPI_Init' has only the error return
.vb
    subroutine MPI_INIT( ierr )
    integer ierr
.ve

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_INIT

.seealso: MPI_Init_thread, MPI_Finalize
@*/
int MPI_Init( int *argc, char ***argv )
{
    static const char FCNAME[] = "MPI_Init";
    int mpi_errno = MPI_SUCCESS;
    int rc ATTRIBUTE((unused));
    int threadLevel, provided;
    MPID_MPI_INIT_STATE_DECL(MPID_STATE_MPI_INIT);

    rc = MPID_Wtime_init();
#ifdef USE_DBG_LOGGING
    MPIU_DBG_PreInit( argc, argv, rc );
#endif

    MPID_MPI_INIT_FUNC_ENTER(MPID_STATE_MPI_INIT);
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
            if (OPA_load_int(&MPIR_Process.mpich_state) != MPICH_PRE_INIT) {
                mpi_errno = MPIR_Err_create_code( MPI_SUCCESS, MPIR_ERR_RECOVERABLE, FCNAME, __LINE__, MPI_ERR_OTHER,
						  "**inittwice", NULL );
	    }
            if (mpi_errno) goto fn_fail;
        }
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */

    /* ... body of routine ... */

    /* Temporarily disable thread-safety.  This is needed because the
     * mutexes are not initialized yet, and we don't want to
     * accidentally use them before they are initialized.  We will
     * reset this value once it is properly initialized. */
#if defined MPICH_IS_THREADED
    MPIR_ThreadInfo.isThreaded = 0;
#endif /* MPICH_IS_THREADED */

    MPIR_T_env_init();

    if (!strcasecmp(MPIR_CVAR_DEFAULT_THREAD_LEVEL, "MPI_THREAD_MULTIPLE"))
        threadLevel = MPI_THREAD_MULTIPLE;
    else if (!strcasecmp(MPIR_CVAR_DEFAULT_THREAD_LEVEL, "MPI_THREAD_SERIALIZED"))
        threadLevel = MPI_THREAD_SERIALIZED;
    else if (!strcasecmp(MPIR_CVAR_DEFAULT_THREAD_LEVEL, "MPI_THREAD_FUNNELED"))
        threadLevel = MPI_THREAD_FUNNELED;
    else if (!strcasecmp(MPIR_CVAR_DEFAULT_THREAD_LEVEL, "MPI_THREAD_SINGLE"))
        threadLevel = MPI_THREAD_SINGLE;
    else {
        MPL_error_printf("Unrecognized thread level %s\n", MPIR_CVAR_DEFAULT_THREAD_LEVEL);
        exit(1);
    }

    /* If the user requested for asynchronous progress, request for
     * THREAD_MULTIPLE. */
    if (MPIR_CVAR_ASYNC_PROGRESS)
        threadLevel = MPI_THREAD_MULTIPLE;

    mpi_errno = MPIR_Init_thread( argc, argv, threadLevel, &provided );
    if (mpi_errno != MPI_SUCCESS) goto fn_fail;

    if (MPIR_CVAR_ASYNC_PROGRESS) {
        if (provided == MPI_THREAD_MULTIPLE) {
            mpi_errno = MPIR_Init_async_thread();
            if (mpi_errno) goto fn_fail;

            MPIR_async_thread_initialized = 1;
        }
        else {
            printf("WARNING: No MPI_THREAD_MULTIPLE support (needed for async progress)\n");
        }
    }

    /* ... end of body of routine ... */
    MPID_MPI_INIT_FUNC_EXIT(MPID_STATE_MPI_INIT);

/*timndus start*/

    int __shared_info = -2;
    MPI_Comm_rank(MPI_COMM_WORLD, &__WORLD_RANK);
    MPI_Comm_size(MPI_COMM_WORLD, &__WORLD_SIZE);

    int __TIM_cpu_id = 0;
    __TIM_cpu_id = sched_getcpu();

    FILE * __fp;
    /*
    __fp = fopen ("/home/timndus/ainfo","a");
    fprintf(__fp, "%d,%d\n", __WORLD_RANK, __TIM_cpu_id);
    fclose (__fp);
    */
    for (int i = 0; i < __WORLD_SIZE; i++)
    {
        __shared_info = __TIM_cpu_id;
        MPI_Bcast(&__shared_info, 1, MPI_INT, i, MPI_COMM_WORLD);
        __SHARED_INFO_ARR[i] = __shared_info;
        MPI_Barrier(MPI_COMM_WORLD);
    }
//////////////////////////////////////////////////////find available freq
    char __scaling_available_frequencies_dir[100];
    char __scaling_available_frequencies_str[256];

    sprintf(__scaling_available_frequencies_dir,"%s%d%s", "/sys/devices/system/cpu/cpu", __SHARED_INFO_ARR[__WORLD_RANK], "/cpufreq/scaling_available_frequencies");
    __fp = fopen(__scaling_available_frequencies_dir, "r");
    if( fgets(__scaling_available_frequencies_str, 256, __fp) == NULL )
    {
        printf("error in read!");
    }
    fclose(__fp);

    char *__char_pt;
    int __i = 0;
    __char_pt = strtok (__scaling_available_frequencies_str," ");
    while (__char_pt != NULL)
    {
        sscanf(__char_pt, "%d", &__SCALING_AVAILABLE_FREQUENCYIES_ARR[__i]);
        __char_pt = strtok (NULL, " ");
        __i++;
    }
/////////////////////////////////////////////////////find donation quota
    char __cpuinfo_cur_freq_dir[60];
    char __cpuinfo_cur_freq_str[8];
    int __cpuinfo_cur_freq = 0;

    sprintf(__cpuinfo_cur_freq_dir,"%s%d%s", "/sys/devices/system/cpu/cpu", __SHARED_INFO_ARR[__WORLD_RANK], "/cpufreq/cpuinfo_cur_freq");
    __fp = fopen(__cpuinfo_cur_freq_dir, "r");
    if( fgets(__cpuinfo_cur_freq_str, 8, __fp) == NULL )
    {
        printf("error in read!");
    }
    fclose(__fp);
    sscanf(__cpuinfo_cur_freq_str, "%d", &__cpuinfo_cur_freq);
    __DONATION_QUOTA = __cpuinfo_cur_freq - __SCALING_AVAILABLE_FREQUENCYIES_ARR[__i - 2];

/*timndus end*/

    return mpi_errno;

  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
#   ifdef HAVE_ERROR_REPORTING
    {
	mpi_errno = MPIR_Err_create_code(
	    mpi_errno, MPIR_ERR_RECOVERABLE, FCNAME, __LINE__, MPI_ERR_OTHER, 
	    "**mpi_init", "**mpi_init %p %p", argc, argv);
    }
#   endif
    mpi_errno = MPIR_Err_return_comm( 0, FCNAME, mpi_errno );
    return mpi_errno;
    /* --END ERROR HANDLING-- */
}
