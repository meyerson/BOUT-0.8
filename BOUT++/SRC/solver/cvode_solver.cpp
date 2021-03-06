/**************************************************************************
 * Interface to PVODE solver
 *
 **************************************************************************
 * Copyright 2010 B.D.Dudson, S.Farley, M.V.Umansky, X.Q.Xu
 *
 * Contact Ben Dudson, bd512@york.ac.uk
 * 
 * This file is part of BOUT++.
 *
 * BOUT++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BOUT++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
 * 
 **************************************************************************/

#include "solver.h"

#include "globals.h"

#include "mpi.h"       // MPI data types and prototypes
#include "nvector.h"
#include "cvode.h"     // main CVODE header file
#include "iterativ.h"  // contains the enum for types of preconditioning
#include "cvspgmr.h"   // use CVSPGMR linear solver each internal step
#include "pvbbdpre.h"  // band preconditioner function prototypes

#include <stdlib.h>

#include "boundary.h"
#include "interpolation.h" // Cell interpolation

void solver_f(integer N, real t, N_Vector u, N_Vector udot, void *f_data);
void solver_gloc(integer N, real t, real* u, real* udot, void *f_data);
void solver_cfn(integer N, real t, N_Vector u, void *f_data);

const real ZERO = 0.0;

static real abstol, reltol; // addresses passed in init must be preserved
static PVBBDData pdata;

long int iopt[OPT_SIZE];
real ropt[OPT_SIZE];

Solver::Solver() : GenericSolver()
{
  gfunc = (rhsfunc) NULL;

  has_constraints = false; ///< This solver doesn't have constraints
}

Solver::~Solver()
{
  if(initialised) {
    // Free CVODE memory
    
    N_VFree(u);
    PVBBDFree(pdata);
    CVodeFree(cvode_mem);
    PVecFreeMPI(machEnv);
  }
}

/**************************************************************************
 * Initialise
 **************************************************************************/

int Solver::init(rhsfunc f, int argc, char **argv, bool restarting, int nout, real tstep)
{
  int mudq, mldq, mukeep, mlkeep;
  boole optIn;
  int i;
  bool use_precon;
  int precon_dimens;
  real precon_tol;

  int n2d = n2Dvars(); // Number of 2D variables
  int n3d = n3Dvars(); // Number of 3D variables

#ifdef CHECK
  int msg_point = msg_stack.push("Initialising PVODE solver");
#endif

  /// Call the generic initialisation first
  if(GenericSolver::init(f, argc, argv, restarting, nout, tstep))
    return 1;
  
  // Save nout and tstep for use in run
  NOUT = nout;
  TIMESTEP = tstep;

  output.write("Initialising PVODE solver\n");
  
  // Set the rhs solver function
  func = f;
  if(gfunc == (rhsfunc) NULL)
    gfunc = f; // If preconditioner function not specified, use f

  int local_N = getLocalN();
  
  // Get total problem size
  int neq;
  if(MPI_Allreduce(&local_N, &neq, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD)) {
    output.write("\tERROR: MPI_Allreduce failed!\n");
    return 1;
  }
  
  output.write("\t3d fields = %d, 2d fields = %d neq=%d, local_N=%d\n",
	       n3d, n2d, neq, local_N);

  // Set machEnv block
  machEnv = (machEnvType) PVecInitMPI(MPI_COMM_WORLD, local_N, neq, &argc, &argv);

  if (machEnv == NULL) {
    if(MYPE == 0)
      output.write("\tError: PVecInitMPI failed\n");
    return(1);
  }

  // Allocate memory, and set problem data, initial values, tolerances

  u = N_VNew(neq, machEnv);

  ///////////// GET OPTIONS /////////////

  int pvode_mxstep;
  
  options.setSection("solver");
  options.get("mudq", mudq, n3d*(MXSUB+2));
  options.get("mldq", mldq, n3d*(MXSUB+2));
  options.get("mukeep", mukeep, 0);
  options.get("mlkeep", mlkeep, 0);
  options.get("ATOL", abstol, 1.0e-12);
  options.get("RTOL", reltol, 1.0e-5);
  options.get("use_precon", use_precon, false);
  options.get("precon_dimens", precon_dimens, 50);
  options.get("precon_tol", precon_tol, 1.0e-4);
  options.get("pvode_mxstep", pvode_mxstep, 500);

  pdata = PVBBDAlloc(local_N, mudq, mldq, mukeep, mlkeep, ZERO, 
                     solver_gloc, solver_cfn, (void*) this);
  
  if (pdata == NULL) {
    output.write("\tError: PVBBDAlloc failed.\n");
    return(1);
  }

  ////////// SAVE DATA TO CVODE ///////////

  // Set pointer to data array in vector u.
  real *udata = N_VDATA(u);
  if(save_vars(udata)) {
    bout_error("\tError: Initial variable value not set\n");
    return(1);
  }
  
  /* Call CVodeMalloc to initialize CVODE: 
     
     neq     is the problem size = number of equations
     f       is the user's right hand side function in y'=f(t,y)
     T0      is the initial time
     u       is the initial dependent variable vector
     BDF     specifies the Backward Differentiation Formula
     NEWTON  specifies a Newton iteration
     SS      specifies scalar relative and absolute tolerances
     &reltol and &abstol are pointers to the scalar tolerances
     data    is the pointer to the user-defined data block
     NULL    is the pointer to the error message file
     FALSE   indicates there are no optional inputs in iopt and ropt
     iopt, ropt  communicate optional integer and real input/output

     A pointer to CVODE problem memory is returned and stored in cvode_mem.  */

  optIn = TRUE; for(i=0;i<OPT_SIZE;i++)iopt[i]=0; 
                for(i=0;i<OPT_SIZE;i++)ropt[i]=ZERO;
		iopt[MXSTEP]=pvode_mxstep;

  cvode_mem = CVodeMalloc(neq, solver_f, simtime, u, BDF, NEWTON, SS, &reltol,
                          &abstol, this, NULL, optIn, iopt, ropt, machEnv);

  if(cvode_mem == NULL) {
    output.write("\tError: CVodeMalloc failed.\n");
    return(1);
  }
  
  /* Call CVSpgmr to specify the CVODE linear solver CVSPGMR with
     left preconditioning, modified Gram-Schmidt orthogonalization,
     default values for the maximum Krylov dimension maxl and the tolerance
     parameter delt, preconditioner setup and solve routines from the
     PVBBDPRE module, and the pointer to the preconditioner data block.    */

  if(use_precon) {
    CVSpgmr(cvode_mem, LEFT, MODIFIED_GS, precon_dimens, precon_tol, PVBBDPrecon, PVBBDPSol, pdata);
  }else {
    CVSpgmr(cvode_mem, NONE, MODIFIED_GS, 10, ZERO, PVBBDPrecon, PVBBDPSol, pdata);
  }

  /*  CVSpgmr(cvode_mem, NONE, MODIFIED_GS, 10, 0.0, PVBBDPrecon, PVBBDPSol, pdata); */
  
  
#ifdef CHECK
  msg_stack.pop(msg_point);
#endif
  
  return(0);
}

/**************************************************************************
 * Run - Advance time
 **************************************************************************/

int Solver::run(MonitorFunc monitor)
{
#ifdef CHECK
  int msg_point = msg_stack.push("CVODE Solver::run()");
#endif
  
  if(!initialised)
    bout_error("Solver not initialised\n");
  
  for(int i=0;i<NOUT;i++) {
    
    /// Run the solver for one output timestep
    simtime = run(simtime + TIMESTEP, rhs_ncalls, rhs_wtime);
    iteration++;

    /// Check if the run succeeded
    if(simtime < 0.0) {
      // Step failed
      output.write("Timestep failed. Aborting\n");

      // Write restart to a different file
      restart.write("%s/BOUT.failed.%d.%s", restartdir.c_str(), MYPE, restartext.c_str());

      bout_error("PVODE timestep failed\n");
    }

    /// Write the restart file
    restart.write("%s/BOUT.restart.%d.%s", restartdir.c_str(), MYPE, restartext.c_str());
    
    if((archive_restart > 0) && (iteration % archive_restart == 0)) {
      restart.write("%s/BOUT.restart_%04d.%d.%s", restartdir.c_str(), iteration, MYPE, restartext.c_str());
    }
    
    /// Call the monitor function
    
    if(monitor(simtime, i, NOUT)) {
      // User signalled to quit
      
      // Write restart to a different file
      restart.write("%s/BOUT.final.%d.%s", restartdir.c_str(), MYPE, restartext.c_str());
      
      output.write("Monitor signalled to quit. Returning\n");
      break;
    }
  }
  
#ifdef CHECK
  msg_stack.pop(msg_point);
#endif

  return 0;
}

real Solver::run(real tout, int &ncalls, real &rhstime)
{
  real *udata;
  int flag;

#ifdef CHECK
  int msg_point = msg_stack.push("Running solver: solver::run(%e)", tout);
#endif

  rhs_wtime = 0.0;
  rhs_ncalls = 0;

  // Set pointer to data array in vector u.
  udata = N_VDATA(u);

  // Run CVODE
  flag = CVode(cvode_mem, tout, u, &simtime, NORMAL);

  ncalls = rhs_ncalls;
  rhstime = rhs_wtime;

  // Copy variables
  load_vars(udata);
  
  // Call rhs function to get extra variables at this time
  real tstart = MPI_Wtime();
  (*func)(simtime);
  rhstime += MPI_Wtime() - tstart;
  ncalls++;

  // Check return flag
  if(flag != SUCCESS) {
    output.write("ERROR CVODE step failed, flag = %d\n", flag);
    return(-1.0);
  }

#ifdef CHECK
  msg_stack.pop(msg_point);
#endif

  return simtime;
}

/**************************************************************************
 * RHS function
 **************************************************************************/

void Solver::rhs(int N, real t, real *udata, real *dudata)
{
  int flag;
  real tstart;

#ifdef CHECK
  int msg_point = msg_stack.push("Running RHS: Solver::rhs(%e)", t);
#endif

  tstart = MPI_Wtime();

  // Load state from CVODE
  load_vars(udata);

  // Call function
  flag = (*func)(t);

  // Save derivatives to CVODE
  save_derivs(dudata);

  rhs_wtime += MPI_Wtime() - tstart;
  rhs_ncalls++;

#ifdef CHECK
  msg_stack.pop(msg_point);
#endif
}

void Solver::gloc(int N, real t, real *udata, real *dudata)
{
  int flag;
  real tstart;

#ifdef CHECK
  int msg_point = msg_stack.push("Running RHS: Solver::gloc(%e)", t);
#endif

  tstart = MPI_Wtime();

  // Load state from CVODE
  load_vars(udata);

  // Call function
  flag = (*gfunc)(t);

  // Save derivatives to CVODE
  save_derivs(dudata);

  rhs_wtime += MPI_Wtime() - tstart;
  rhs_ncalls++;

#ifdef CHECK
  msg_stack.pop(msg_point);
#endif
}

/**************************************************************************
 * PRIVATE FUNCTIONS
 **************************************************************************/

/// Perform an operation at a given (jx,jy) location, moving data between BOUT++ and CVODE
void Solver::loop_vars_op(int jx, int jy, real *udata, int &p, SOLVER_VAR_OP op)
{
  real **d2d, ***d3d;
  unsigned int i;
  int jz;

  int n2d = n2Dvars();
  int n3d = n3Dvars();
 
  switch(op) {
  case LOAD_VARS: {
    /// Load variables from CVODE into BOUT++
    
    // Loop over 2D variables
    for(i=0;i<n2d;i++) {
      d2d = f2d[i].var->getData(); // Get pointer to data
      d2d[jx][jy] = udata[p];
      p++;
    }
    
    for (jz=0; jz < ncz; jz++) {
      
      // Loop over 3D variables
      for(i=0;i<n3d;i++) {
	d3d = f3d[i].var->getData(); // Get pointer to data
	d3d[jx][jy][jz] = udata[p];
	p++;
      }
    }
    break;
  }
  case SAVE_VARS: {
    /// Save variables from BOUT++ into CVODE (only used at start of simulation)
    
    // Loop over 2D variables
    for(i=0;i<n2d;i++) {
      d2d = f2d[i].var->getData(); // Get pointer to data
      udata[p] = d2d[jx][jy];
      p++;
    }
    
    for (jz=0; jz < ncz; jz++) {
      
      // Loop over 3D variables
      for(i=0;i<n3d;i++) {
	d3d = f3d[i].var->getData(); // Get pointer to data
	udata[p] = d3d[jx][jy][jz];
	p++;
      }  
    }
    break;
  }
    /// Save time-derivatives from BOUT++ into CVODE (returning RHS result)
  case SAVE_DERIVS: {
    
    // Loop over 2D variables
    for(i=0;i<n2d;i++) {
      d2d = f2d[i].F_var->getData(); // Get pointer to data
      udata[p] = d2d[jx][jy];
      p++;
    }
    
    for (jz=0; jz < ncz; jz++) {
      
      // Loop over 3D variables
      for(i=0;i<n3d;i++) {
	d3d = f3d[i].F_var->getData(); // Get pointer to data
	udata[p] = d3d[jx][jy][jz];
	p++;
      }  
    }
    break;
  }
  }
}

/// Loop over variables and domain. Used for all data operations for consistency
void Solver::loop_vars(real *udata, SOLVER_VAR_OP op)
{
  int jx, jy;
  int p = 0; // Counter for location in udata array

  // Inner X boundary
  if(IDATA_DEST == -1) {
    for(jx=0;jx<MXG;jx++)
      for(jy=0;jy<MYSUB;jy++)
	loop_vars_op(jx, jy+MYG, udata, p, op);
  }

  for (jx=MXG; jx < MXSUB+MXG; jx++) {
    
    // Lower Y boundary region
    
    if( ((DDATA_INDEST == -1) && (jx < DDATA_XSPLIT)) ||
	((DDATA_OUTDEST == -1) && (jx >= DDATA_XSPLIT)) ) {
      for(jy=0;jy<MYG;jy++)
	loop_vars_op(jx, jy, udata, p, op);
    }
    
    for (jy=0; jy < MYSUB; jy++) {
      // Bulk of points
      loop_vars_op(jx, jy+MYG, udata, p, op);
    }

    // Upper Y boundary condition

    if( ((UDATA_INDEST == -1) && (jx < UDATA_XSPLIT)) ||
	((UDATA_OUTDEST == -1) && (jx >= UDATA_XSPLIT)) ) {
      for(jy=0;jy<MYG;jy++)
	loop_vars_op(jx, MYSUB+MYG+jy, udata, p, op);
    }
  }

  // Outer X boundary
  if(ODATA_DEST == -1) {
    for(jx=0;jx<MXG;jx++)
      for(jy=0;jy<MYSUB;jy++)
	loop_vars_op(MXG+MXSUB+jx, jy+MYG, udata, p, op);
  }
}

void Solver::load_vars(real *udata)
{
  unsigned int i;
  
  // Make sure data is allocated
  for(i=0;i<f2d.size();i++)
    f2d[i].var->Allocate();
  for(i=0;i<f3d.size();i++) {
    f3d[i].var->Allocate();
    f3d[i].var->setLocation(f3d[i].location);
  }

  loop_vars(udata, LOAD_VARS);

  // Mark each vector as either co- or contra-variant

  for(i=0;i<v2d.size();i++)
    v2d[i].var->covariant = v2d[i].covariant;
  for(i=0;i<v3d.size();i++)
    v3d[i].var->covariant = v3d[i].covariant;
}

// This function only called during initialisation
int Solver::save_vars(real *udata)
{
  unsigned int i;

  for(i=0;i<f2d.size();i++)
    if(f2d[i].var->getData() == (real**) NULL)
      return(1);

  for(i=0;i<f3d.size();i++)
    if(f3d[i].var->getData() == (real***) NULL)
      return(1);
  
  // Make sure vectors in correct basis
  for(i=0;i<v2d.size();i++) {
    if(v2d[i].covariant) {
      v2d[i].var->to_covariant();
    }else
      v2d[i].var->to_contravariant();
  }
  for(i=0;i<v3d.size();i++) {
    if(v3d[i].covariant) {
      v3d[i].var->to_covariant();
    }else
      v3d[i].var->to_contravariant();
  }

  loop_vars(udata, SAVE_VARS);

  return(0);
}

void Solver::save_derivs(real *dudata)
{
  unsigned int i;

  // Make sure vectors in correct basis
  for(i=0;i<v2d.size();i++) {
    if(v2d[i].covariant) {
      v2d[i].F_var->to_covariant();
    }else
      v2d[i].F_var->to_contravariant();
  }
  for(i=0;i<v3d.size();i++) {
    if(v3d[i].covariant) {
      v3d[i].F_var->to_covariant();
    }else
      v3d[i].F_var->to_contravariant();
  }

  // Make sure 3D fields are at the correct cell location
  for(vector< VarStr<Field3D> >::iterator it = f3d.begin(); it != f3d.end(); it++) {
    if((*it).location != ((*it).F_var)->getLocation()) {
      //output.write("SOLVER: Interpolating\n");
      *((*it).F_var) = interp_to(*((*it).F_var), (*it).location);
    }
  }

  loop_vars(dudata, SAVE_DERIVS);
}

/**************************************************************************
 * CVODE rhs function
 **************************************************************************/

void solver_f(integer N, real t, N_Vector u, N_Vector udot, void *f_data)
{
  real *udata, *dudata;
  Solver *s;

  udata = N_VDATA(u);
  dudata = N_VDATA(udot);
  
  s = (Solver*) f_data;

  s->rhs(N, t, udata, dudata);
}

// Preconditioner RHS
void solver_gloc(integer N, real t, real* u, real* udot, void *f_data)
{
  Solver *s;
  
  s = (Solver*) f_data;

  s->gloc(N, t, u, udot);
}

// Preconditioner communication function
void solver_cfn(integer N, real t, N_Vector u, void *f_data)
{
  // doesn't do anything at the moment
}
