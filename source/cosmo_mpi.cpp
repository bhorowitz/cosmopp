#ifdef COSMO_MPI
#include <mpi.h>
#endif

#include <cosmo_mpi.hpp>
#include <macros.hpp>

CosmoMPI::CosmoMPI()
{
#ifdef COSMO_MPI
#ifdef CHECKS_ON
    int hasMpiInitialized;
    MPI_Initialized(&hasMpiInitialized);
    check(!hasMpiInitialized, "MPI already initialized");
#endif

    MPI_Init(NULL, NULL);
#endif
    commTag_ = 1000;
}

CosmoMPI::~CosmoMPI()
{
#ifdef COSMO_MPI
#ifdef CHECKS_ON
    int hasMpiInitialized;
    MPI_Initialized(&hasMpiInitialized);
    check(hasMpiInitialized, "MPI not initialized");

    int hasMpiFinalized;
    MPI_Finalized(&hasMpiFinalized);
    check(!hasMpiFinalized, "MPI already finalized");
#endif
    MPI_Finalize();
#endif
}

int
CosmoMPI::processId() const
{
#ifdef COSMO_MPI
#ifdef CHECKS_ON
    int hasMpiInitialized;
    MPI_Initialized(&hasMpiInitialized);
    check(hasMpiInitialized, "MPI not initialized");

    int hasMpiFinalized;
    MPI_Finalized(&hasMpiFinalized);
    check(!hasMpiFinalized, "MPI already finalized");
#endif
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return rank;
#else
    return 0;
#endif
}

int
CosmoMPI::numProcesses() const
{
#ifdef COSMO_MPI
#ifdef CHECKS_ON
    int hasMpiInitialized;
    MPI_Initialized(&hasMpiInitialized);
    check(hasMpiInitialized, "MPI not initialized");

    int hasMpiFinalized;
    MPI_Finalized(&hasMpiFinalized);
    check(!hasMpiFinalized, "MPI already finalized");
#endif
    int n;
    MPI_Comm_size(MPI_COMM_WORLD, &n);
    return n;
#else
    return 1;
#endif
}

void
CosmoMPI::barrier() const
{
#ifdef COSMO_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif
}

int
CosmoMPI::getCommTag()
{
    barrier();

    commTag_ += 10 * numProcesses();

    return commTag_;
}
