/*!
 * \file CFEABoundVariable.cpp
 * \brief Definition of the variables for FEM elastic structural problems.
 * \author R. Sanchez
 * \version 6.2.0 "Falcon"
 *
 * The current SU2 release has been coordinated by the
 * SU2 International Developers Society <www.su2devsociety.org>
 * with selected contributions from the open-source community.
 *
 * The main research teams contributing to the current release are:
 *  - Prof. Juan J. Alonso's group at Stanford University.
 *  - Prof. Piero Colonna's group at Delft University of Technology.
 *  - Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *  - Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *  - Prof. Rafael Palacios' group at Imperial College London.
 *  - Prof. Vincent Terrapon's group at the University of Liege.
 *  - Prof. Edwin van der Weide's group at the University of Twente.
 *  - Lab. of New Concepts in Aeronautics at Tech. Institute of Aeronautics.
 *
 * Copyright 2012-2019, Francisco D. Palacios, Thomas D. Economon,
 *                      Tim Albring, and the SU2 contributors.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../include/variables/CFEABoundVariable.hpp"


CFEABoundVariable::CFEABoundVariable(const su2double *val_fea, Idx_t npoint, Idx_t ndim, Idx_t nvar, CConfig *config)
  : CFEAVariable(val_fea, npoint, ndim, nvar, config) {

  VertexMap.Reset(nPoint);
}

void CFEABoundVariable::AllocateBoundaryVariables(CConfig *config) {

  if (VertexMap.GetIsValid()) return; // nothing to do

  /*--- Count number of vertices and build map ---*/

  Idx_t nBoundPt = VertexMap.Build();

  /*--- Allocate ---*/

  bool gen_alpha = (config->GetKind_TimeIntScheme_FEA() == GENERALIZED_ALPHA);
  fsi_analysis   = (config->GetnMarker_Fluid_Load() > 0); // FSI simulation

  /*--- Surface residual ---*/
  Residual_Ext_Surf.resize(nBoundPt,nVar) = su2double(0.0);

  /*--- Flow traction ---*/
  if (fsi_analysis) FlowTraction.resize(nBoundPt,nVar) = su2double(0.0);

  /*--- Generalized alpha integration method requires storing the old residuals ---*/
  if (gen_alpha) {
    Residual_Ext_Surf_n.resize(nBoundPt,nVar) = su2double(0.0);

    if (fsi_analysis) FlowTraction_n.resize(nBoundPt,nVar) = su2double(0.0);
  }
}

void CFEABoundVariable::Set_FlowTraction_n() { FlowTraction_n = FlowTraction; }

void CFEABoundVariable::Set_SurfaceLoad_Res_n() { Residual_Ext_Surf_n = Residual_Ext_Surf; }

void CFEABoundVariable::Clear_FlowTraction() { FlowTraction.setConstant(0.0); }

void CFEABoundVariable::RegisterFlowTraction() {
  if (!fsi_analysis) return;
  for (Idx_t iVertex = 0; iVertex < FlowTraction.rows(); iVertex++)
    for (Idx_t iVar = 0; iVar < nVar; iVar++)
      AD::RegisterInput(FlowTraction(iVertex,iVar));
}
