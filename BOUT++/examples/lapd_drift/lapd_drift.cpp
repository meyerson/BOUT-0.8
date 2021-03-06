/*******************************************************************************
 * 2-fluid equations
 * Same as Maxim's version of BOUT - simplified 2-fluid for benchmarking
 *******************************************************************************/

#include "bout.h"
#include "initialprofiles.h"
#include "derivs.h"
#include "interpolation.h"
#include "invert_laplace.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// 2D initial profiles
Field2D Ni0, Ti0, Te0, Vi0, phi0, Ve0, rho0, Ajpar0;
Vector2D b0xcv; // for curvature terms

// 3D evolving fields
Field3D rho,Ni, Ajpar;

// 3D time-derivatives
Field3D F_rho, F_Ni, F_Ajpar;

// Derived 3D variables
Field3D phi, Apar, Ve, jpar;

// Non-linear coefficients
Field3D nu, mu_i, kapa_Te, kapa_Ti;

// 3D total values
Field3D Nit, Tit, Tet, Vit;

// pressures
Field3D pei, pe;
Field2D pei0, pe0;

// Metric coefficients
Field2D Rxy, Bpxy, Btxy, hthe;

// parameters
real Te_x, Ti_x, Ni_x, Vi_x, bmag, rho_s, fmei, AA, ZZ;
real lambda_ei, lambda_ii;
real nu_hat, mui_hat, wci, nueix, nuiix;
real beta_p;
real nuIonNeutral; // Ion-neutral collision rate (normalised by wci)

// settings
bool estatic, ZeroElMass; // Switch for electrostatic operation (true = no Apar)

bool bout_exb;  // Use BOUT-06 expression for ExB velocity

real zeff, nu_perp;
bool evolve_rho,evolve_ni, evolve_ajpar;
real ShearFactor;

bool nonlinear;

bool filter_z;
int filter_z_mode;

int phi_flags, apar_flags; // Inversion flags

bool relax_flat_bndry; // If true, relax the boundaries flat

bool niprofile;

bool evolve_source; // If true, evolve a source/sink profile
real source_response;  // Initial source response (inverse timescale) 
real source_converge;  // Timescale for convergence
Field2D Sn, F_Sn; // Density source (inverse timescale)
bool input_source; // Read Sn from the input file

// Communication object
Communicator comms;
Communicator com_jp;

// BOUT-06 L1
const Field3D Div_par_CtoL(const Field3D &var)
{
  return Bxy * Grad_par_CtoL(var / Bxy);
}

int physics_init()
{
  Field2D I; // Shear factor 
  
  output.write("Solving 6-variable 2-fluid equations\n");

  /************* LOAD DATA FROM GRID FILE ****************/

  // Load 2D profiles (set to zero if not found)
  grid_load2d(Ni0,    "Ni0");
  grid_load2d(Ti0,    "Ti0");
  grid_load2d(Te0,    "Te0");
  grid_load2d(Vi0,    "Vi0");
  grid_load2d(Ve0,    "Ve0");
  grid_load2d(phi0,   "phi0");
  grid_load2d(rho0,   "rho0");
  grid_load2d(Ajpar0, "Ajpar0");

  // Load magnetic curvature term
  b0xcv.covariant = false; // Read contravariant components
  grid_load2d(b0xcv, "bxcv"); // b0xkappa terms

  // Load metrics
  grid_load2d(Rxy,  "Rxy");
  grid_load2d(Bpxy, "Bpxy");
  grid_load2d(Btxy, "Btxy");
  grid_load2d(hthe, "hthe");
  grid_load2d(dx,   "dpsi");
  grid_load2d(I,    "sinty");
  grid_load2d(zShift, "qinty");

  // Load normalisation values
  grid_load(Te_x, "Te_x");
  grid_load(Ti_x, "Ti_x");
  grid_load(Ni_x, "Ni_x");
  grid_load(bmag, "bmag");

  Ni_x *= 1.0e14;
  bmag *= 1.0e4;

  /*************** READ OPTIONS *************************/

  // Read some parameters
  options.setSection("2fluid");
  options.get("AA", AA, 2.0);
  options.get("ZZ", ZZ, 1.0);

  options.get("estatic",     estatic,     false);
  options.get("ZeroElMass",  ZeroElMass,  false);
  options.get("Zeff",        zeff,        1.0);
  options.get("nu_perp",     nu_perp,     0.0);
  OPTION(ShearFactor, 1.0); // <=> options.get("ShearFactor", ShearFactor, 1.0);
  OPTION(nuIonNeutral, -1.); 
  OPTION(bout_exb,    false);
  
  OPTION(niprofile, false);
  OPTION(evolve_source, false);
  OPTION(source_response, 1.0);
  OPTION(source_converge, 100.);
  
  OPTION(input_source, false);

  options.get("phi_flags",   phi_flags,   0);
  options.get("apar_flags",  apar_flags,  0);

  OPTION(relax_flat_bndry, false);

  OPTION(nonlinear, true);
  
  // Toroidal filtering
  OPTION(filter_z,          false);  // Filter a single n
  OPTION(filter_z_mode,     1);

  options.get("rho",   "evolve", evolve_rho,   true);
  options.get("Ni",    "evolve", evolve_ni,    true);
  options.get("Ajpar", "evolve", evolve_ajpar, true);

  if(ZeroElMass)
    evolve_ajpar = 0; // Don't need ajpar - calculated from ohm's law

  /************* SHIFTED RADIAL COORDINATES ************/

  if(ShiftXderivs) {
    ShearFactor = 0.0;  // I disappears from metric
    b0xcv.z += I*b0xcv.x;
  }

  /************** CALCULATE PARAMETERS *****************/

  rho_s = 1.02*sqrt(AA*Te_x)/ZZ/bmag;
  fmei  = 1./1836.2/AA;

  lambda_ei = 24.-log(sqrt(Ni_x)/Te_x);
  lambda_ii = 23.-log(ZZ*ZZ*ZZ*sqrt(2.*Ni_x)/pow(Ti_x, 1.5));
  wci       = 9.58e3*ZZ*bmag/AA;
  nueix     = 2.91e-6*Ni_x*lambda_ei/pow(Te_x, 1.5);
  nuiix     = 4.78e-8*pow(ZZ,4.)*Ni_x*lambda_ii/pow(Ti_x, 1.5)/sqrt(AA);
  nu_hat    = zeff*nueix/wci;

  if(nu_perp < 1.e-10) {
    mui_hat      = (3./10.)*nuiix/wci;
  } else
    mui_hat      = nu_perp;

  if(estatic) {
    beta_p    = 1.e-29;
  }else
    beta_p    = 4.03e-11*Ni_x*Te_x/bmag/bmag;

  Vi_x = wci * rho_s;

  output.write("Collisions: nueix = %e, nu_hat = %e\n", nueix, nu_hat);

  /************** PRINT Z INFORMATION ******************/
  
  real hthe0;
  if(grid_load(hthe0, "hthe0") == 0) {
    output.write("    ****NOTE: input from BOUT, Z length needs to be divided by %e\n", hthe0/rho_s);
  }

  /************** SHIFTED GRIDS LOCATION ***************/

  // Velocities defined on cell boundaries
  Ajpar.setLocation(CELL_YLOW);

  // Apar and jpar too
  Apar.setLocation(CELL_YLOW); 
  jpar.setLocation(CELL_YLOW);

  /************** NORMALISE QUANTITIES *****************/

  output.write("\tNormalising to rho_s = %e\n", rho_s);

  // Normalise profiles
  Ni0 /= Ni_x/1.0e14;
  Ti0 /= Te_x;
  Te0 /= Te_x;
  phi0 /= Te_x;
  Vi0 /= Vi_x;

  // Normalise curvature term
  b0xcv.x /= (bmag/1e4);
  b0xcv.y *= rho_s*rho_s;
  b0xcv.z *= rho_s*rho_s;
  
  // Normalise geometry 
  Rxy /= rho_s;
  hthe /= rho_s;
  I *= rho_s*rho_s*(bmag/1e4)*ShearFactor;
  dx /= rho_s*rho_s*(bmag/1e4);

  // Normalise magnetic field
  Bpxy /= (bmag/1.e4);
  Btxy /= (bmag/1.e4);
  Bxy  /= (bmag/1.e4);

  // calculate pressures
  pei0 = (Ti0 + Te0)*Ni0;
  pe0 = Te0*Ni0;

  /**************** CALCULATE METRICS ******************/

  g11 = (Rxy*Bpxy)^2;
  g22 = 1.0 / (hthe^2);
  g33 = (I^2)*g11 + (Bxy^2)/g11;
  g12 = 0.0;
  g13 = -I*g11;
  g23 = -Btxy/(hthe*Bpxy*Rxy);
  
  J = hthe / Bpxy;
  
  g_11 = 1.0/g11 + ((I*Rxy)^2);
  g_22 = (Bxy*hthe/Bpxy)^2;
  g_33 = Rxy*Rxy;
  g_12 = Btxy*hthe*I*Rxy/Bpxy;
  g_13 = I*Rxy*Rxy;
  g_23 = Btxy*hthe*Rxy/Bpxy;

  // Twist-shift. NOTE: Should really use qsafe rather than qinty (small correction)

  if((jyseps2_2 / MYSUB) == MYPE) {
    for(int i=0;i<ngx;i++)
      ShiftAngle[i] = zShift[i][MYSUB]; // MYSUB+MYG-1
  }
  if(NYPE > 1)
    MPI_Bcast(ShiftAngle, ngx, PVEC_REAL_MPI_TYPE,jyseps2_2/MYSUB, MPI_COMM_WORLD);

  /**************** SET EVOLVING VARIABLES *************/

  // Tell BOUT++ which variables to evolve
  // add evolving variables to the communication object
  if(evolve_rho) {
    bout_solve(rho,   F_rho,   "rho");
    comms.add(rho);
    output.write("rho\n");
  }else
    initial_profile("rho", rho);

  if(evolve_ni) {
    bout_solve(Ni,    F_Ni,    "Ni");
    comms.add(Ni);
    output.write("ni\n");
  }else
    initial_profile("Ni", Ni);

  if(evolve_ajpar) {
    bout_solve(Ajpar, F_Ajpar, "Ajpar");
    comms.add(Ajpar);
    output.write("ajpar\n");
  }else {
    initial_profile("Ajpar", Ajpar);
    if(ZeroElMass)
      dump.add(Ajpar, "Ajpar", 1); // output calculated Ajpar
  }

  if(evolve_source) {
    bout_solve(Sn, F_Sn, "Sn");
  }
  if(input_source)
    grid_load2d(Sn, "Sn");

  if(!restarting) {
    // Ensure boundary condition is satisfied for initial condition
    apply_boundary(rho, "rho");
    apply_boundary(Ni, "Ni");
    apply_boundary(Ajpar, "Ajpar");
  }
  
  /************** SETUP COMMUNICATIONS **************/

  // add extra variables to communication
  comms.add(phi);
  comms.add(Apar);

  // Add any other variables to be dumped to file
  dump.add(phi,  "phi",  1);
  dump.add(Apar, "Apar", 1);
  dump.add(jpar, "jpar", 1);

  dump.add(Ni0, "Ni0", 0);
  dump.add(Te0, "Te0", 0);
  dump.add(Ti0, "Ti0", 0);

  dump.add(Te_x,  "Te_x", 0);
  dump.add(Ti_x,  "Ti_x", 0);
  dump.add(Ni_x,  "Ni_x", 0);
  dump.add(rho_s, "rho_s", 0);
  dump.add(wci,   "wci", 0);

  
  //dump.add(F_Ni, "F_Ni", 1);
  //dump.add(F_rho, "F_rho", 1);
  //dump.add(F_Ajpar, "F_Ajpar", 1);

  com_jp.add(jpar);
  
  return(0);
}

// Routines for ExB terms (end of this file)
const Field2D vE_Grad(const Field2D &f, const Field2D &p);
const Field3D vE_Grad(const Field2D &f, const Field3D &p);
const Field3D vE_Grad(const Field3D &f, const Field2D &p);
const Field3D vE_Grad(const Field3D &f, const Field3D &p);

int physics_run(real t)
{
  // Invert vorticity to get phi
  
  // Solves \nabla^2_\perp x + (1./c)*\nabla_perp c\cdot\nabla_\perp x + a x = b
  // Arguments are:   (b,   bit-field, a,    c)
  // Passing NULL -> missing term
  if(nonlinear) {
    phi = invert_laplace(rho/(Ni0+Ni), phi_flags, NULL, &Ni0);
  }else
    phi = invert_laplace(rho/Ni0, phi_flags, NULL, &Ni0);

  if(estatic || ZeroElMass) {
    // Electrostatic operation
    Apar = 0.0;
  }else {
    // Invert Ajpar to get Apar
    static Field2D acoeff;
    static bool aset = false;
    
    if(!aset) // calculate Apar coefficient
      acoeff = (-0.5*beta_p/fmei)*Ni0;
    aset = true;
  
    Apar = invert_laplace(-acoeff*Ajpar, apar_flags, &acoeff);
  }

  // Communicate variables
  comms.run();

  // Update profiles
  if(nonlinear) {
    Nit = Ni0 + Ni;
    Tit = Ti0;
    Tet = Te0;
  }else {
    Nit = Ni0;  //+ Ni.DC();
    Tit = Ti0; // + Ti.DC();
    Tet = Te0; // + Te.DC();
  }

  real source_alpha;
  
  // Calculate source response
  if(source_converge > 0.) {
      source_alpha = source_response * exp(-1.*t/source_converge);
  }else
      source_alpha = source_response;

  // Update non-linear coefficients
  nu      = nu_hat * Nit / (Tet^1.5);
  mu_i    = mui_hat * Nit / (Tit^0.5);
  kapa_Te = 3.2*(1./fmei)*(wci/nueix)*(Tet^2.5);
  kapa_Ti = 3.9*(wci/nuiix)*(Tit^2.5);
  
  // Calculate pressures
  pei = (Tet+Tit)*Nit;
  pe  = Tet*Nit;
  
  if(ZeroElMass) {
    // Set jpar,Ve,Ajpar neglecting the electron inertia term
    //jpar = ((Te0*Grad_par(Ni, CELL_YLOW)) - (Ni0*Grad_par(phi, CELL_YLOW)))/(fmei*0.51*nu);
    jpar = ((Tet*Grad_par_LtoC(Ni)) - (Nit*Grad_par_LtoC(phi)))/(fmei*0.51*nu);
    
    // Set radial boundary condition on jpar
    bndry_inner_flat(jpar);
    bndry_sol_flat(jpar);
    bndry_toroidal(jpar);
    
    // Need to communicate jpar
    com_jp.run();

    Ve = -jpar/Nit;
    Ajpar = Ve;
  }else {
    
    Ve = Ajpar + Apar;
    jpar = -Nit*Ve;
  }

  // DENSITY EQUATION

  F_Ni = 0.0;
  if(evolve_ni) {
    F_Ni -= vE_Grad(Ni0, phi);
    
    if(nonlinear)
      F_Ni -= vE_Grad(Ni, phi);
    
    F_Ni += Div_par_CtoL(jpar); // Left hand differencing

    if(evolve_source || input_source) {
      // Evolve source
      if(evolve_source)
        F_Sn = average_y(-1. * source_alpha * Ni.DC() / Ni0);

      // Add density source/sink
      F_Ni += Sn*where(Sn, Ni0, Nit); // Sn*Ni0 if Sn > 0, Sn*Nit if Sn < 0

    }else if(niprofile) {
      // Allowing Ni profile to change
      if(PE_XIND == 0) {
	// Inner boundary
	for(int i=0;i<3;i++) {
	  // Relax upwards (only add density)
	  for(int j=0;j<ngy;j++)
	    for(int k=0;k<ngz;k++) {
	      if(Ni[i][j][k] < 0.0)
		F_Ni[i][j][k] -= 0.1*Ni[i][j][k];
	      }
	    }
      }
      if(PE_XIND == NXPE-1) {
	// Outer boundary
	for(int i=0;i<3;i++) {
	  // Relax downwards (only remove density)
	  for(int j=0;j<ngy;j++)
	    for(int k=0;k<ngz;k++) {
	      if(Ni[ngx-1-i][j][k] > 0.0)
		F_Ni[ngx-1-i][j][k] -= 0.1*Ni[ngx-1-i][j][k];
	      }
	}
      }
    }else
      F_Ni -= F_Ni.DC(); // REMOVE TOROIDAL AVERAGE DENSITY
  }

  // VORTICITY

  F_rho = 0.0;
  if(evolve_rho) {
    
    if(nonlinear)
      F_rho -= vE_Grad(rho, phi);

    //F_rho += Bxy*Bxy*Div_par(jpar, CELL_CENTRE);
    F_rho += Bxy*Bxy*Div_par_CtoL(jpar); // Left hand differencing

    if(nuIonNeutral > 0.0)
      F_rho -= nuIonNeutral * rho;
    
    if(evolve_source || input_source) {
      // Sinks also remove vorticity
      F_rho += Sn*where(Sn, 0., rho);
    }
  }
  
  // AJPAR

  F_Ajpar = 0.0;
  if(evolve_ajpar) {

    //F_Ajpar += (1./fmei)*Grad_par(phi, CELL_YLOW);
    F_Ajpar += (1./fmei)*Grad_par_LtoC(phi); // Right-hand differencing

    //F_Ajpar -= (1./fmei)*(Tet/Nit)*Grad_par(Ni, CELL_YLOW);
    F_Ajpar -= (1./fmei)*(Tet/Nit)*Grad_par_LtoC(Ni);
    
    F_Ajpar += 0.51*nu*jpar/Ni0;
  }

  // Z filtering
  if(filter_z) {
    // Filter out all except filter_z_mode
    
    F_rho = filter(F_rho, filter_z_mode);
    F_Ni = filter(F_Ni, filter_z_mode);
    F_Ajpar = filter(F_Ajpar, filter_z_mode);
  }

  // BOUNDARY CONDITIONS
  
  if(relax_flat_bndry) {
    bndry_inner_relax_flat(F_rho, rho);
    bndry_sol_relax_flat(F_rho, rho);

    bndry_inner_relax_flat(F_Ni, Ni);
    bndry_sol_relax_flat(F_Ni, Ni);

    bndry_inner_relax_flat(F_Ajpar, Ajpar);
    bndry_sol_relax_flat(F_Ajpar, Ajpar);
  }else {
    apply_boundary(F_rho, "rho");
    apply_boundary(F_Ni, "Ni");
    apply_boundary(F_Ajpar, "Ajpar");
  }

  return(0);
}

/////////////////////////////////////////////////////////////////
// ExB terms. These routines allow comparisons with BOUT-06
// if bout_exb=true is set in BOUT.inp
/////////////////////////////////////////////////////////////////

const Field2D vE_Grad(const Field2D &f, const Field2D &p)
{
  Field2D result;
  if(bout_exb) {
    // Use a subset of terms for comparison to BOUT-06
    result = 0.0;
  }else {
    // Use full expression with all terms
    result = b0xGrad_dot_Grad(p, f) / Bxy;
  }
  return result;
}

const Field3D vE_Grad(const Field2D &f, const Field3D &p)
{
  Field3D result;
  if(bout_exb) {
    // Use a subset of terms for comparison to BOUT-06
    result = VDDX(DDZ(p), f);
  }else {
    // Use full expression with all terms
    result = b0xGrad_dot_Grad(p, f) / Bxy;
  }
  return result;
}

const Field3D vE_Grad(const Field3D &f, const Field2D &p)
{
  Field3D result;
  if(bout_exb) {
    // Use a subset of terms for comparison to BOUT-06
    result = VDDZ(-DDX(p), f);
  }else {
    // Use full expression with all terms
    result = b0xGrad_dot_Grad(p, f) / Bxy;
  }
  return result;
}

const Field3D vE_Grad(const Field3D &f, const Field3D &p)
{
  Field3D result;
  if(bout_exb) {
    // Use a subset of terms for comparison to BOUT-06
    result = VDDX(DDZ(p), f) + VDDZ(-DDX(p), f);
  }else {
    // Use full expression with all terms
    result = b0xGrad_dot_Grad(p, f) / Bxy;
  }
  return result;
}
