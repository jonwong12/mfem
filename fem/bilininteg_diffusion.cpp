// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#include "../general/forall.hpp"
#include "bilininteg.hpp"
#include "gridfunc.hpp"

using namespace std;

namespace mfem
{

// PA Diffusion Integrator

// OCCA 2D Assemble kernel
#ifdef MFEM_USE_OCCA
static void OccaPADiffusionSetup2D(const int D1D,
                                   const int Q1D,
                                   const int NE,
                                   const Array<double> &W,
                                   const Vector &J,
                                   const double COEFF,
                                   Vector &op)
{
   occa::properties props;
   props["defines/D1D"] = D1D;
   props["defines/Q1D"] = Q1D;
   const occa::memory o_W = OccaMemoryRead(W.GetMemory(), W.Size());
   const occa::memory o_J = OccaMemoryRead(J.GetMemory(), J.Size());
   occa::memory o_op = OccaMemoryWrite(op.GetMemory(), op.Size());
   const occa_id_t id = std::make_pair(D1D,Q1D);
   static occa_kernel_t OccaDiffSetup2D_ker;
   if (OccaDiffSetup2D_ker.find(id) == OccaDiffSetup2D_ker.end())
   {
      const occa::kernel DiffusionSetup2D =
         mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                     "DiffusionSetup2D", props);
      OccaDiffSetup2D_ker.emplace(id, DiffusionSetup2D);
   }
   OccaDiffSetup2D_ker.at(id)(NE, o_W, o_J, COEFF, o_op);
}

static void OccaPADiffusionSetup3D(const int D1D,
                                   const int Q1D,
                                   const int NE,
                                   const Array<double> &W,
                                   const Vector &J,
                                   const double COEFF,
                                   Vector &op)
{
   occa::properties props;
   props["defines/D1D"] = D1D;
   props["defines/Q1D"] = Q1D;
   const occa::memory o_W = OccaMemoryRead(W.GetMemory(), W.Size());
   const occa::memory o_J = OccaMemoryRead(J.GetMemory(), J.Size());
   occa::memory o_op = OccaMemoryWrite(op.GetMemory(), op.Size());
   const occa_id_t id = std::make_pair(D1D,Q1D);
   static occa_kernel_t OccaDiffSetup3D_ker;
   if (OccaDiffSetup3D_ker.find(id) == OccaDiffSetup3D_ker.end())
   {
      const occa::kernel DiffusionSetup3D =
         mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                     "DiffusionSetup3D", props);
      OccaDiffSetup3D_ker.emplace(id, DiffusionSetup3D);
   }
   OccaDiffSetup3D_ker.at(id)(NE, o_W, o_J, COEFF, o_op);
}
#endif // MFEM_USE_OCCA

// PA Diffusion Assemble 2D kernel
static void PADiffusionSetup2D(const int Q1D,
                               const int NE,
                               const Array<double> &w,
                               const Vector &j,
                               const double COEFF,
                               Vector &op)
{
   const int NQ = Q1D*Q1D;
   auto W = w.Read();

   auto J = Reshape(j.Read(), NQ, 2, 2, NE);
   auto y = Reshape(op.Write(), NQ, 3, NE);

   MFEM_FORALL(e, NE,
   {
      for (int q = 0; q < NQ; ++q)
      {
         const double J11 = J(q,0,0,e);
         const double J21 = J(q,1,0,e);
         const double J12 = J(q,0,1,e);
         const double J22 = J(q,1,1,e);
         const double c_detJ = W[q] * COEFF / ((J11*J22)-(J21*J12));
         y(q,0,e) =  c_detJ * (J12*J12 + J22*J22); // 1,1
         y(q,1,e) = -c_detJ * (J12*J11 + J22*J21); // 1,2
         y(q,2,e) =  c_detJ * (J11*J11 + J21*J21); // 2,2
      }
   });
}

// PA Diffusion Assemble 3D kernel
static void PADiffusionSetup3D(const int Q1D,
                               const int NE,
                               const Array<double> &w,
                               const Vector &j,
                               const double COEFF,
                               Vector &op)
{
   const int NQ = Q1D*Q1D*Q1D;
   auto W = w.Read();
   auto J = Reshape(j.Read(), NQ, 3, 3, NE);
   auto y = Reshape(op.Write(), NQ, 6, NE);
   MFEM_FORALL(e, NE,
   {
      for (int q = 0; q < NQ; ++q)
      {
         const double J11 = J(q,0,0,e);
         const double J21 = J(q,1,0,e);
         const double J31 = J(q,2,0,e);
         const double J12 = J(q,0,1,e);
         const double J22 = J(q,1,1,e);
         const double J32 = J(q,2,1,e);
         const double J13 = J(q,0,2,e);
         const double J23 = J(q,1,2,e);
         const double J33 = J(q,2,2,e);
         const double detJ = J11 * (J22 * J33 - J32 * J23) -
         /* */               J21 * (J12 * J33 - J32 * J13) +
         /* */               J31 * (J12 * J23 - J22 * J13);
         const double c_detJ = W[q] * COEFF / detJ;
         // adj(J)
         const double A11 = (J22 * J33) - (J23 * J32);
         const double A12 = (J32 * J13) - (J12 * J33);
         const double A13 = (J12 * J23) - (J22 * J13);
         const double A21 = (J31 * J23) - (J21 * J33);
         const double A22 = (J11 * J33) - (J13 * J31);
         const double A23 = (J21 * J13) - (J11 * J23);
         const double A31 = (J21 * J32) - (J31 * J22);
         const double A32 = (J31 * J12) - (J11 * J32);
         const double A33 = (J11 * J22) - (J12 * J21);
         // detJ J^{-1} J^{-T} = (1/detJ) adj(J) adj(J)^T
         y(q,0,e) = c_detJ * (A11*A11 + A12*A12 + A13*A13); // 1,1
         y(q,1,e) = c_detJ * (A11*A21 + A12*A22 + A13*A23); // 2,1
         y(q,2,e) = c_detJ * (A11*A31 + A12*A32 + A13*A33); // 3,1
         y(q,3,e) = c_detJ * (A21*A21 + A22*A22 + A23*A23); // 2,2
         y(q,4,e) = c_detJ * (A21*A31 + A22*A32 + A23*A33); // 3,2
         y(q,5,e) = c_detJ * (A31*A31 + A32*A32 + A33*A33); // 3,3
      }
   });
}

static void PADiffusionSetup(const int dim,
                             const int D1D,
                             const int Q1D,
                             const int NE,
                             const Array<double> &W,
                             const Vector &J,
                             const double COEFF,
                             Vector &op)
{
   if (dim == 1) { MFEM_ABORT("dim==1 not supported in PADiffusionSetup"); }
   if (dim == 2)
   {
#ifdef MFEM_USE_OCCA
      if (DeviceCanUseOcca())
      {
         OccaPADiffusionSetup2D(D1D, Q1D, NE, W, J, COEFF, op);
         return;
      }
#endif // MFEM_USE_OCCA
      PADiffusionSetup2D(Q1D, NE, W, J, COEFF, op);
   }
   if (dim == 3)
   {
#ifdef MFEM_USE_OCCA
      if (DeviceCanUseOcca())
      {
         OccaPADiffusionSetup3D(D1D, Q1D, NE, W, J, COEFF, op);
         return;
      }
#endif // MFEM_USE_OCCA
      PADiffusionSetup3D(Q1D, NE, W, J, COEFF, op);
   }
}

void DiffusionIntegrator::Setup(const FiniteElementSpace &fes)
{
   // Assumes tensor-product elements
   Mesh *mesh = fes.GetMesh();
   const FiniteElement &el = *fes.GetFE(0);
   const IntegrationRule *ir = IntRule ? IntRule : &GetRule(el, el);
   const int dims = el.GetDim();
   const int symmDims = (dims * (dims + 1)) / 2; // 1x1: 1, 2x2: 3, 3x3: 6
   const int nq = ir->GetNPoints();
   dim = mesh->Dimension();
   ne = fes.GetNE();
   geom = mesh->GetGeometricFactors(*ir, GeometricFactors::JACOBIANS);
   maps = &el.GetDofToQuad(*ir, DofToQuad::TENSOR);
   dofs1D = maps->ndof;
   quad1D = maps->nqpt;
   pa_data.SetSize(symmDims * nq * ne, Device::GetMemoryType());
   double coeff = 1.0;
   if (Q)
   {
      ConstantCoefficient *cQ = dynamic_cast<ConstantCoefficient*>(Q);
      MFEM_VERIFY(cQ != NULL, "only ConstantCoefficient is supported!");
      coeff = cQ->constant;
   }
   PADiffusionSetup(dim, dofs1D, quad1D, ne, ir->GetWeights(), geom->J,
                    coeff, pa_data);
}

#ifdef MFEM_USE_OCCA
// OCCA PA Diffusion Apply 2D kernel
static void OccaPADiffusionApply2D(const int D1D,
                                   const int Q1D,
                                   const int NE,
                                   const Array<double> &B,
                                   const Array<double> &G,
                                   const Array<double> &Bt,
                                   const Array<double> &Gt,
                                   const Vector &op,
                                   const Vector &x,
                                   Vector &y)
{
   occa::properties props;
   props["defines/D1D"] = D1D;
   props["defines/Q1D"] = Q1D;
   const occa::memory o_B = OccaMemoryRead(B.GetMemory(), B.Size());
   const occa::memory o_G = OccaMemoryRead(G.GetMemory(), G.Size());
   const occa::memory o_Bt = OccaMemoryRead(Bt.GetMemory(), Bt.Size());
   const occa::memory o_Gt = OccaMemoryRead(Gt.GetMemory(), Gt.Size());
   const occa::memory o_op = OccaMemoryRead(op.GetMemory(), op.Size());
   const occa::memory o_x = OccaMemoryRead(x.GetMemory(), x.Size());
   occa::memory o_y = OccaMemoryReadWrite(y.GetMemory(), y.Size());
   const occa_id_t id = std::make_pair(D1D,Q1D);
   if (!Device::Allows(Backend::OCCA_CUDA))
   {
      static occa_kernel_t OccaDiffApply2D_cpu;
      if (OccaDiffApply2D_cpu.find(id) == OccaDiffApply2D_cpu.end())
      {
         const occa::kernel DiffusionApply2D_CPU =
            mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                        "DiffusionApply2D_CPU", props);
         OccaDiffApply2D_cpu.emplace(id, DiffusionApply2D_CPU);
      }
      OccaDiffApply2D_cpu.at(id)(NE, o_B, o_G, o_Bt, o_Gt, o_op, o_x, o_y);
   }
   else
   {
      static occa_kernel_t OccaDiffApply2D_gpu;
      if (OccaDiffApply2D_gpu.find(id) == OccaDiffApply2D_gpu.end())
      {
         const occa::kernel DiffusionApply2D_GPU =
            mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                        "DiffusionApply2D_GPU", props);
         OccaDiffApply2D_gpu.emplace(id, DiffusionApply2D_GPU);
      }
      OccaDiffApply2D_gpu.at(id)(NE, o_B, o_G, o_Bt, o_Gt, o_op, o_x, o_y);
   }
}

// OCCA PA Diffusion Apply 3D kernel
static void OccaPADiffusionApply3D(const int D1D,
                                   const int Q1D,
                                   const int NE,
                                   const Array<double> &B,
                                   const Array<double> &G,
                                   const Array<double> &Bt,
                                   const Array<double> &Gt,
                                   const Vector &op,
                                   const Vector &x,
                                   Vector &y)
{
   occa::properties props;
   props["defines/D1D"] = D1D;
   props["defines/Q1D"] = Q1D;
   const occa::memory o_B = OccaMemoryRead(B.GetMemory(), B.Size());
   const occa::memory o_G = OccaMemoryRead(G.GetMemory(), G.Size());
   const occa::memory o_Bt = OccaMemoryRead(Bt.GetMemory(), Bt.Size());
   const occa::memory o_Gt = OccaMemoryRead(Gt.GetMemory(), Gt.Size());
   const occa::memory o_op = OccaMemoryRead(op.GetMemory(), op.Size());
   const occa::memory o_x = OccaMemoryRead(x.GetMemory(), x.Size());
   occa::memory o_y = OccaMemoryReadWrite(y.GetMemory(), y.Size());
   const occa_id_t id = std::make_pair(D1D,Q1D);
   if (!Device::Allows(Backend::OCCA_CUDA))
   {
      static occa_kernel_t OccaDiffApply3D_cpu;
      if (OccaDiffApply3D_cpu.find(id) == OccaDiffApply3D_cpu.end())
      {
         const occa::kernel DiffusionApply3D_CPU =
            mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                        "DiffusionApply3D_CPU", props);
         OccaDiffApply3D_cpu.emplace(id, DiffusionApply3D_CPU);
      }
      OccaDiffApply3D_cpu.at(id)(NE, o_B, o_G, o_Bt, o_Gt, o_op, o_x, o_y);
   }
   else
   {
      static occa_kernel_t OccaDiffApply3D_gpu;
      if (OccaDiffApply3D_gpu.find(id) == OccaDiffApply3D_gpu.end())
      {
         const occa::kernel DiffusionApply3D_GPU =
            mfem::OccaDev().buildKernel("occa://mfem/fem/occa.okl",
                                        "DiffusionApply3D_GPU", props);
         OccaDiffApply3D_gpu.emplace(id, DiffusionApply3D_GPU);
      }
      OccaDiffApply3D_gpu.at(id)(NE, o_B, o_G, o_Bt, o_Gt, o_op, o_x, o_y);
   }
}
#endif // MFEM_USE_OCCA


// PA Diffusion Apply 2D kernel
#ifndef MFEM_USE_JIT
template<const int T_D1D = 0,
         const int T_Q1D = 0>
static void PADiffusionApply2D(const int NE,
                               const Array<double> &b,
                               const Array<double> &g,
                               const Array<double> &bt,
                               const Array<double> &gt,
                               const Vector &_op,
                               const Vector &_x,
                               Vector &_y,
                               const int d1d = 0,
                               const int q1d = 0)
{
   const int D1D = T_D1D ? T_D1D : d1d;
   const int Q1D = T_Q1D ? T_Q1D : q1d;
   MFEM_VERIFY(D1D <= MAX_D1D, "");
   MFEM_VERIFY(Q1D <= MAX_Q1D, "");
   auto B = Reshape(b.Read(), Q1D, D1D);
   auto G = Reshape(g.Read(), Q1D, D1D);
   auto Bt = Reshape(bt.Read(), D1D, Q1D);
   auto Gt = Reshape(gt.Read(), D1D, Q1D);
   auto op = Reshape(_op.Read(), Q1D*Q1D, 3, NE);
   auto x = Reshape(_x.Read(), D1D, D1D, NE);
   auto y = Reshape(_y.ReadWrite(), D1D, D1D, NE);
   MFEM_FORALL(e, NE,
   {
      const int D1D = T_D1D ? T_D1D : d1d;
      const int Q1D = T_Q1D ? T_Q1D : q1d;
      // the following variables are evaluated at compile time
      constexpr int max_D1D = T_D1D ? T_D1D : MAX_D1D;
      constexpr int max_Q1D = T_Q1D ? T_Q1D : MAX_Q1D;

      double grad[max_Q1D][max_Q1D][2];
      for (int qy = 0; qy < Q1D; ++qy)
      {
         for (int qx = 0; qx < Q1D; ++qx)
         {
            grad[qy][qx][0] = 0.0;
            grad[qy][qx][1] = 0.0;
         }
      }
      for (int dy = 0; dy < D1D; ++dy)
      {
         double gradX[max_Q1D][2];
         for (int qx = 0; qx < Q1D; ++qx)
         {
            gradX[qx][0] = 0.0;
            gradX[qx][1] = 0.0;
         }
         for (int dx = 0; dx < D1D; ++dx)
         {
            const double s = x(dx,dy,e);
            for (int qx = 0; qx < Q1D; ++qx)
            {
               gradX[qx][0] += s * B(qx,dx);
               gradX[qx][1] += s * G(qx,dx);
            }
         }
         for (int qy = 0; qy < Q1D; ++qy)
         {
            const double wy  = B(qy,dy);
            const double wDy = G(qy,dy);
            for (int qx = 0; qx < Q1D; ++qx)
            {
               grad[qy][qx][0] += gradX[qx][1] * wy;
               grad[qy][qx][1] += gradX[qx][0] * wDy;
            }
         }
      }
      // Calculate Dxy, xDy in plane
      for (int qy = 0; qy < Q1D; ++qy)
      {
         for (int qx = 0; qx < Q1D; ++qx)
         {
            const int q = qx + qy * Q1D;

            const double O11 = op(q,0,e);
            const double O12 = op(q,1,e);
            const double O22 = op(q,2,e);

            const double gradX = grad[qy][qx][0];
            const double gradY = grad[qy][qx][1];

            grad[qy][qx][0] = (O11 * gradX) + (O12 * gradY);
            grad[qy][qx][1] = (O12 * gradX) + (O22 * gradY);
         }
      }
      for (int qy = 0; qy < Q1D; ++qy)
      {
         double gradX[max_D1D][2];
         for (int dx = 0; dx < D1D; ++dx)
         {
            gradX[dx][0] = 0;
            gradX[dx][1] = 0;
         }
         for (int qx = 0; qx < Q1D; ++qx)
         {
            const double gX = grad[qy][qx][0];
            const double gY = grad[qy][qx][1];
            for (int dx = 0; dx < D1D; ++dx)
            {
               const double wx  = Bt(dx,qx);
               const double wDx = Gt(dx,qx);
               gradX[dx][0] += gX * wDx;
               gradX[dx][1] += gY * wx;
            }
         }
         for (int dy = 0; dy < D1D; ++dy)
         {
            const double wy  = Bt(dy,qy);
            const double wDy = Gt(dy,qy);
            for (int dx = 0; dx < D1D; ++dx)
            {
               y(dx,dy,e) += ((gradX[dx][0] * wy) + (gradX[dx][1] * wDy));
            }
         }
      }
   });
}
#endif

// Shared memory PA Diffusion Apply 2D kernel
MFEM_JIT
template<const int T_D1D = 0,
         const int T_Q1D = 0,
         const int T_NBZ = 0>
static void SmemPADiffusionApply2D(const int NE,
                                   const Array<double> &_b,
                                   const Array<double> &_g,
                                   const Vector &_op,
                                   const Vector &_x,
                                   Vector &_y,
                                   const int d1d = 0,
                                   const int q1d = 0,
                                   const int nbz = 0)
{
   const int D1D = T_D1D ? T_D1D : d1d;
   const int Q1D = T_Q1D ? T_Q1D : q1d;
   constexpr int NBZ = T_NBZ ? T_NBZ : 1;
   constexpr int MQ1 = T_Q1D ? T_Q1D : MAX_Q1D;
   constexpr int MD1 = T_D1D ? T_D1D : MAX_D1D;
   MFEM_VERIFY(D1D <= MD1, "");
   MFEM_VERIFY(Q1D <= MQ1, "");
   auto b = Reshape(_b.Read(), Q1D, D1D);
   auto g = Reshape(_g.Read(), Q1D, D1D);
   auto op = Reshape(_op.Read(), Q1D*Q1D, 3, NE);
   auto x = Reshape(_x.Read(), D1D, D1D, NE);
   auto y = Reshape(_y.ReadWrite(), D1D, D1D, NE);
   MFEM_FORALL_2D(e, NE, Q1D, Q1D, NBZ,
   {
      const int tidz = MFEM_THREAD_ID(z);
      const int D1D = T_D1D ? T_D1D : d1d;
      const int Q1D = T_Q1D ? T_Q1D : q1d;
      constexpr int NBZ = T_NBZ ? T_NBZ : 1;
      constexpr int MQ1 = T_Q1D ? T_Q1D : MAX_Q1D;
      constexpr int MD1 = T_D1D ? T_D1D : MAX_D1D;
      MFEM_SHARED double sBG[2][MQ1*MD1];
      double (*B)[MD1] = (double (*)[MD1]) (sBG+0);
      double (*G)[MD1] = (double (*)[MD1]) (sBG+1);
      double (*Bt)[MQ1] = (double (*)[MQ1]) (sBG+0);
      double (*Gt)[MQ1] = (double (*)[MQ1]) (sBG+1);
      MFEM_SHARED double Xz[NBZ][MD1][MD1];
      MFEM_SHARED double GD[2][NBZ][MD1][MQ1];
      MFEM_SHARED double GQ[2][NBZ][MD1][MQ1];
      double (*X)[MD1] = (double (*)[MD1])(Xz + tidz);
      double (*DQ0)[MD1] = (double (*)[MD1])(GD[0] + tidz);
      double (*DQ1)[MD1] = (double (*)[MD1])(GD[1] + tidz);
      double (*QQ0)[MD1] = (double (*)[MD1])(GQ[0] + tidz);
      double (*QQ1)[MD1] = (double (*)[MD1])(GQ[1] + tidz);
      MFEM_FOREACH_THREAD(dy,y,D1D)
      {
         MFEM_FOREACH_THREAD(dx,x,D1D)
         {
            X[dy][dx] = x(dx,dy,e);
         }
      }
      if (tidz == 0)
      {
         MFEM_FOREACH_THREAD(d,y,D1D)
         {
            MFEM_FOREACH_THREAD(q,x,Q1D)
            {
               B[q][d] = b(q,d);
               G[q][d] = g(q,d);
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(dy,y,D1D)
      {
         MFEM_FOREACH_THREAD(qx,x,Q1D)
         {
            double u = 0.0;
            double v = 0.0;
            for (int dx = 0; dx < D1D; ++dx)
            {
               const double coords = X[dy][dx];
               u += B[qx][dx] * coords;
               v += G[qx][dx] * coords;
            }
            DQ0[dy][qx] = u;
            DQ1[dy][qx] = v;
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(qy,y,Q1D)
      {
         MFEM_FOREACH_THREAD(qx,x,Q1D)
         {
            double u = 0.0;
            double v = 0.0;
            for (int dy = 0; dy < D1D; ++dy)
            {
               u += DQ1[dy][qx] * B[qy][dy];
               v += DQ0[dy][qx] * G[qy][dy];
            }
            QQ0[qy][qx] = u;
            QQ1[qy][qx] = v;
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(qy,y,Q1D)
      {
         MFEM_FOREACH_THREAD(qx,x,Q1D)
         {
            const int q = (qx + ((qy) * Q1D));
            const double O11 = op(q,0,e);
            const double O12 = op(q,1,e);
            const double O22 = op(q,2,e);
            const double gX = QQ0[qy][qx];
            const double gY = QQ1[qy][qx];
            QQ0[qy][qx] = (O11 * gX) + (O12 * gY);
            QQ1[qy][qx] = (O12 * gX) + (O22 * gY);
         }
      }
      MFEM_SYNC_THREAD;
      if (tidz == 0)
      {
         MFEM_FOREACH_THREAD(d,y,D1D)
         {
            MFEM_FOREACH_THREAD(q,x,Q1D)
            {
               Bt[d][q] = b(q,d);
               Gt[d][q] = g(q,d);
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(qy,y,Q1D)
      {
         MFEM_FOREACH_THREAD(dx,x,D1D)
         {
            double u = 0.0;
            double v = 0.0;
            for (int qx = 0; qx < Q1D; ++qx)
            {
               u += Gt[dx][qx] * QQ0[qy][qx];
               v += Bt[dx][qx] * QQ1[qy][qx];
            }
            DQ0[qy][dx] = u;
            DQ1[qy][dx] = v;
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(dy,y,D1D)
      {
         MFEM_FOREACH_THREAD(dx,x,D1D)
         {
            double u = 0.0;
            double v = 0.0;
            for (int qy = 0; qy < Q1D; ++qy)
            {
               u += DQ0[qy][dx] * Bt[dy][qy];
               v += DQ1[qy][dx] * Gt[dy][qy];
            }
            y(dx,dy,e) += (u + v);
         }
      }
   });
}


// PA Diffusion Apply 3D kernel
#ifndef MFEM_USE_JIT
template<const int T_D1D = 0,
         const int T_Q1D = 0>
static void PADiffusionApply3D(const int NE,
                               const Array<double> &b,
                               const Array<double> &g,
                               const Array<double> &bt,
                               const Array<double> &gt,
                               const Vector &_op,
                               const Vector &_x,
                               Vector &_y,
                               const int d1d = 0,
                               const int q1d = 0)
{
   const int D1D = T_D1D ? T_D1D : d1d;
   const int Q1D = T_Q1D ? T_Q1D : q1d;
   MFEM_VERIFY(D1D <= MAX_D1D, "");
   MFEM_VERIFY(Q1D <= MAX_Q1D, "");
   auto B = Reshape(b.Read(), Q1D, D1D);
   auto G = Reshape(g.Read(), Q1D, D1D);
   auto Bt = Reshape(bt.Read(), D1D, Q1D);
   auto Gt = Reshape(gt.Read(), D1D, Q1D);
   auto op = Reshape(_op.Read(), Q1D*Q1D*Q1D, 6, NE);
   auto x = Reshape(_x.Read(), D1D, D1D, D1D, NE);
   auto y = Reshape(_y.ReadWrite(), D1D, D1D, D1D, NE);
   MFEM_FORALL(e, NE,
   {
      const int D1D = T_D1D ? T_D1D : d1d;
      const int Q1D = T_Q1D ? T_Q1D : q1d;
      constexpr int max_D1D = T_D1D ? T_D1D : MAX_D1D;
      constexpr int max_Q1D = T_Q1D ? T_Q1D : MAX_Q1D;
      double grad[max_Q1D][max_Q1D][max_Q1D][3];
      for (int qz = 0; qz < Q1D; ++qz)
      {
         for (int qy = 0; qy < Q1D; ++qy)
         {
            for (int qx = 0; qx < Q1D; ++qx)
            {
               grad[qz][qy][qx][0] = 0.0;
               grad[qz][qy][qx][1] = 0.0;
               grad[qz][qy][qx][2] = 0.0;
            }
         }
      }
      for (int dz = 0; dz < D1D; ++dz)
      {
         double gradXY[max_Q1D][max_Q1D][3];
         for (int qy = 0; qy < Q1D; ++qy)
         {
            for (int qx = 0; qx < Q1D; ++qx)
            {
               gradXY[qy][qx][0] = 0.0;
               gradXY[qy][qx][1] = 0.0;
               gradXY[qy][qx][2] = 0.0;
            }
         }
         for (int dy = 0; dy < D1D; ++dy)
         {
            double gradX[max_Q1D][2];
            for (int qx = 0; qx < Q1D; ++qx)
            {
               gradX[qx][0] = 0.0;
               gradX[qx][1] = 0.0;
            }
            for (int dx = 0; dx < D1D; ++dx)
            {
               const double s = x(dx,dy,dz,e);
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  gradX[qx][0] += s * B(qx,dx);
                  gradX[qx][1] += s * G(qx,dx);
               }
            }
            for (int qy = 0; qy < Q1D; ++qy)
            {
               const double wy  = B(qy,dy);
               const double wDy = G(qy,dy);
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  const double wx  = gradX[qx][0];
                  const double wDx = gradX[qx][1];
                  gradXY[qy][qx][0] += wDx * wy;
                  gradXY[qy][qx][1] += wx  * wDy;
                  gradXY[qy][qx][2] += wx  * wy;
               }
            }
         }
         for (int qz = 0; qz < Q1D; ++qz)
         {
            const double wz  = B(qz,dz);
            const double wDz = G(qz,dz);
            for (int qy = 0; qy < Q1D; ++qy)
            {
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  grad[qz][qy][qx][0] += gradXY[qy][qx][0] * wz;
                  grad[qz][qy][qx][1] += gradXY[qy][qx][1] * wz;
                  grad[qz][qy][qx][2] += gradXY[qy][qx][2] * wDz;
               }
            }
         }
      }
      // Calculate Dxyz, xDyz, xyDz in plane
      for (int qz = 0; qz < Q1D; ++qz)
      {
         for (int qy = 0; qy < Q1D; ++qy)
         {
            for (int qx = 0; qx < Q1D; ++qx)
            {
               const int q = qx + (qy + qz * Q1D) * Q1D;
               const double O11 = op(q,0,e);
               const double O12 = op(q,1,e);
               const double O13 = op(q,2,e);
               const double O22 = op(q,3,e);
               const double O23 = op(q,4,e);
               const double O33 = op(q,5,e);
               const double gradX = grad[qz][qy][qx][0];
               const double gradY = grad[qz][qy][qx][1];
               const double gradZ = grad[qz][qy][qx][2];
               grad[qz][qy][qx][0] = (O11*gradX)+(O12*gradY)+(O13*gradZ);
               grad[qz][qy][qx][1] = (O12*gradX)+(O22*gradY)+(O23*gradZ);
               grad[qz][qy][qx][2] = (O13*gradX)+(O23*gradY)+(O33*gradZ);
            }
         }
      }
      for (int qz = 0; qz < Q1D; ++qz)
      {
         double gradXY[max_D1D][max_D1D][3];
         for (int dy = 0; dy < D1D; ++dy)
         {
            for (int dx = 0; dx < D1D; ++dx)
            {
               gradXY[dy][dx][0] = 0;
               gradXY[dy][dx][1] = 0;
               gradXY[dy][dx][2] = 0;
            }
         }
         for (int qy = 0; qy < Q1D; ++qy)
         {
            double gradX[max_D1D][3];
            for (int dx = 0; dx < D1D; ++dx)
            {
               gradX[dx][0] = 0;
               gradX[dx][1] = 0;
               gradX[dx][2] = 0;
            }
            for (int qx = 0; qx < Q1D; ++qx)
            {
               const double gX = grad[qz][qy][qx][0];
               const double gY = grad[qz][qy][qx][1];
               const double gZ = grad[qz][qy][qx][2];
               for (int dx = 0; dx < D1D; ++dx)
               {
                  const double wx  = Bt(dx,qx);
                  const double wDx = Gt(dx,qx);
                  gradX[dx][0] += gX * wDx;
                  gradX[dx][1] += gY * wx;
                  gradX[dx][2] += gZ * wx;
               }
            }
            for (int dy = 0; dy < D1D; ++dy)
            {
               const double wy  = Bt(dy,qy);
               const double wDy = Gt(dy,qy);
               for (int dx = 0; dx < D1D; ++dx)
               {
                  gradXY[dy][dx][0] += gradX[dx][0] * wy;
                  gradXY[dy][dx][1] += gradX[dx][1] * wDy;
                  gradXY[dy][dx][2] += gradX[dx][2] * wy;
               }
            }
         }
         for (int dz = 0; dz < D1D; ++dz)
         {
            const double wz  = Bt(dz,qz);
            const double wDz = Gt(dz,qz);
            for (int dy = 0; dy < D1D; ++dy)
            {
               for (int dx = 0; dx < D1D; ++dx)
               {
                  y(dx,dy,dz,e) +=
                     ((gradXY[dy][dx][0] * wz) +
                      (gradXY[dy][dx][1] * wz) +
                      (gradXY[dy][dx][2] * wDz));
               }
            }
         }
      }
   });
}
#endif

// Shared memory PA Diffusion Apply 3D kernel
MFEM_JIT
template<const int T_D1D = 0,
         const int T_Q1D = 0>
static void SmemPADiffusionApply3D(const int NE,
                                   const Array<double> &_b,
                                   const Array<double> &_g,
                                   const Vector &_op,
                                   const Vector &_x,
                                   Vector &_y,
                                   const int d1d = 0,
                                   const int q1d = 0)
{
   const int D1D = T_D1D ? T_D1D : d1d;
   const int Q1D = T_Q1D ? T_Q1D : q1d;
   constexpr int MQ1 = T_Q1D ? T_Q1D : MAX_Q1D;
   constexpr int MD1 = T_D1D ? T_D1D : MAX_D1D;
   MFEM_VERIFY(D1D <= MD1, "");
   MFEM_VERIFY(Q1D <= MQ1, "");
   auto b = Reshape(_b.Read(), Q1D, D1D);
   auto g = Reshape(_g.Read(), Q1D, D1D);
   auto op = Reshape(_op.Read(), Q1D*Q1D*Q1D, 6, NE);
   auto x = Reshape(_x.Read(), D1D, D1D, D1D, NE);
   auto y = Reshape(_y.ReadWrite(), D1D, D1D, D1D, NE);
   MFEM_FORALL_3D(e, NE, Q1D, Q1D, Q1D,
   {
      const int tidz = MFEM_THREAD_ID(z);
      const int D1D = T_D1D ? T_D1D : d1d;
      const int Q1D = T_Q1D ? T_Q1D : q1d;
      constexpr int MQ1 = T_Q1D ? T_Q1D : MAX_Q1D;
      constexpr int MD1 = T_D1D ? T_D1D : MAX_D1D;
      constexpr int MDQ = MQ1 > MD1 ? MQ1 : MD1;
      MFEM_SHARED double sBG[2][MQ1*MD1];
      double (*B)[MD1] = (double (*)[MD1]) (sBG+0);
      double (*G)[MD1] = (double (*)[MD1]) (sBG+1);
      double (*Bt)[MQ1] = (double (*)[MQ1]) (sBG+0);
      double (*Gt)[MQ1] = (double (*)[MQ1]) (sBG+1);
      MFEM_SHARED double sm0[3][MDQ*MDQ*MDQ];
      MFEM_SHARED double sm1[3][MDQ*MDQ*MDQ];
      double (*X)[MD1][MD1]    = (double (*)[MD1][MD1]) (sm0+2);
      double (*DDQ0)[MD1][MQ1] = (double (*)[MD1][MQ1]) (sm0+0);
      double (*DDQ1)[MD1][MQ1] = (double (*)[MD1][MQ1]) (sm0+1);
      double (*DQQ0)[MQ1][MQ1] = (double (*)[MQ1][MQ1]) (sm1+0);
      double (*DQQ1)[MQ1][MQ1] = (double (*)[MQ1][MQ1]) (sm1+1);
      double (*DQQ2)[MQ1][MQ1] = (double (*)[MQ1][MQ1]) (sm1+2);
      double (*QQQ0)[MQ1][MQ1] = (double (*)[MQ1][MQ1]) (sm0+0);
      double (*QQQ1)[MQ1][MQ1] = (double (*)[MQ1][MQ1]) (sm0+1);
      double (*QQQ2)[MQ1][MQ1] = (double (*)[MQ1][MQ1]) (sm0+2);
      double (*QQD0)[MQ1][MD1] = (double (*)[MQ1][MD1]) (sm1+0);
      double (*QQD1)[MQ1][MD1] = (double (*)[MQ1][MD1]) (sm1+1);
      double (*QQD2)[MQ1][MD1] = (double (*)[MQ1][MD1]) (sm1+2);
      double (*QDD0)[MD1][MD1] = (double (*)[MD1][MD1]) (sm0+0);
      double (*QDD1)[MD1][MD1] = (double (*)[MD1][MD1]) (sm0+1);
      double (*QDD2)[MD1][MD1] = (double (*)[MD1][MD1]) (sm0+2);
      MFEM_FOREACH_THREAD(dz,z,D1D)
      {
         MFEM_FOREACH_THREAD(dy,y,D1D)
         {
            MFEM_FOREACH_THREAD(dx,x,D1D)
            {
               X[dz][dy][dx] = x(dx,dy,dz,e);
            }
         }
      }
      if (tidz == 0)
      {
         MFEM_FOREACH_THREAD(d,y,D1D)
         {
            MFEM_FOREACH_THREAD(q,x,Q1D)
            {
               B[q][d] = b(q,d);
               G[q][d] = g(q,d);
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(dz,z,D1D)
      {
         MFEM_FOREACH_THREAD(dy,y,D1D)
         {
            MFEM_FOREACH_THREAD(qx,x,Q1D)
            {
               double u = 0.0;
               double v = 0.0;
               for (int dx = 0; dx < D1D; ++dx)
               {
                  const double coords = X[dz][dy][dx];
                  u += coords * B[qx][dx];
                  v += coords * G[qx][dx];
               }
               DDQ0[dz][dy][qx] = u;
               DDQ1[dz][dy][qx] = v;
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(dz,z,D1D)
      {
         MFEM_FOREACH_THREAD(qy,y,Q1D)
         {
            MFEM_FOREACH_THREAD(qx,x,Q1D)
            {
               double u = 0.0;
               double v = 0.0;
               double w = 0.0;
               for (int dy = 0; dy < D1D; ++dy)
               {
                  u += DDQ1[dz][dy][qx] * B[qy][dy];
                  v += DDQ0[dz][dy][qx] * G[qy][dy];
                  w += DDQ0[dz][dy][qx] * B[qy][dy];
               }
               DQQ0[dz][qy][qx] = u;
               DQQ1[dz][qy][qx] = v;
               DQQ2[dz][qy][qx] = w;
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(qz,z,Q1D)
      {
         MFEM_FOREACH_THREAD(qy,y,Q1D)
         {
            MFEM_FOREACH_THREAD(qx,x,Q1D)
            {
               double u = 0.0;
               double v = 0.0;
               double w = 0.0;
               for (int dz = 0; dz < D1D; ++dz)
               {
                  u += DQQ0[dz][qy][qx] * B[qz][dz];
                  v += DQQ1[dz][qy][qx] * B[qz][dz];
                  w += DQQ2[dz][qy][qx] * G[qz][dz];
               }
               QQQ0[qz][qy][qx] = u;
               QQQ1[qz][qy][qx] = v;
               QQQ2[qz][qy][qx] = w;
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(qz,z,Q1D)
      {
         MFEM_FOREACH_THREAD(qy,y,Q1D)
         {
            MFEM_FOREACH_THREAD(qx,x,Q1D)
            {
               const int q = qx + ((qy*Q1D) + (qz*Q1D*Q1D));
               const double O11 = op(q,0,e);
               const double O12 = op(q,1,e);
               const double O13 = op(q,2,e);
               const double O22 = op(q,3,e);
               const double O23 = op(q,4,e);
               const double O33 = op(q,5,e);
               const double gX = QQQ0[qz][qy][qx];
               const double gY = QQQ1[qz][qy][qx];
               const double gZ = QQQ2[qz][qy][qx];
               QQQ0[qz][qy][qx] = (O11*gX) + (O12*gY) + (O13*gZ);
               QQQ1[qz][qy][qx] = (O12*gX) + (O22*gY) + (O23*gZ);
               QQQ2[qz][qy][qx] = (O13*gX) + (O23*gY) + (O33*gZ);
            }
         }
      }
      MFEM_SYNC_THREAD;
      if (tidz == 0)
      {
         MFEM_FOREACH_THREAD(d,y,D1D)
         {
            MFEM_FOREACH_THREAD(q,x,Q1D)
            {
               Bt[d][q] = b(q,d);
               Gt[d][q] = g(q,d);
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(qz,z,Q1D)
      {
         MFEM_FOREACH_THREAD(qy,y,Q1D)
         {
            MFEM_FOREACH_THREAD(dx,x,D1D)
            {
               double u = 0.0;
               double v = 0.0;
               double w = 0.0;
               for (int qx = 0; qx < Q1D; ++qx)
               {
                  u += QQQ0[qz][qy][qx] * Gt[dx][qx];
                  v += QQQ1[qz][qy][qx] * Bt[dx][qx];
                  w += QQQ2[qz][qy][qx] * Bt[dx][qx];
               }
               QQD0[qz][qy][dx] = u;
               QQD1[qz][qy][dx] = v;
               QQD2[qz][qy][dx] = w;
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(qz,z,Q1D)
      {
         MFEM_FOREACH_THREAD(dy,y,D1D)
         {
            MFEM_FOREACH_THREAD(dx,x,D1D)
            {
               double u = 0.0;
               double v = 0.0;
               double w = 0.0;
               for (int qy = 0; qy < Q1D; ++qy)
               {
                  u += QQD0[qz][qy][dx] * Bt[dy][qy];
                  v += QQD1[qz][qy][dx] * Gt[dy][qy];
                  w += QQD2[qz][qy][dx] * Bt[dy][qy];
               }
               QDD0[qz][dy][dx] = u;
               QDD1[qz][dy][dx] = v;
               QDD2[qz][dy][dx] = w;
            }
         }
      }
      MFEM_SYNC_THREAD;
      MFEM_FOREACH_THREAD(dz,z,D1D)
      {
         MFEM_FOREACH_THREAD(dy,y,D1D)
         {
            MFEM_FOREACH_THREAD(dx,x,D1D)
            {
               double u = 0.0;
               double v = 0.0;
               double w = 0.0;
               for (int qz = 0; qz < Q1D; ++qz)
               {
                  u += QDD0[qz][dy][dx] * Bt[dz][qz];
                  v += QDD1[qz][dy][dx] * Bt[dz][qz];
                  w += QDD2[qz][dy][dx] * Gt[dz][qz];
               }
               y(dx,dy,dz,e) += (u + v + w);
            }
         }
      }
   });
}

#define for_Q(k)\
{\
   MFEM_SYNC_THREAD;\
   MFEM_FOREACH_THREAD(j,y,Q1D)\
   {\
      MFEM_FOREACH_THREAD(i,x,Q1D)\
      {\
         double qr = 0.0, qs = 0.0;\
         MFEM_EXCLUSIVE_GET(r_qt) = 0.0;\
         for (int m = 0; m < Q1D; m++)\
         {\
            double Dim = s_D[i][m];\
            double Djm = s_D[j][m];\
            double Dkm = s_D[k][m];\
            qr += Dim*s_Iq[k][j][m];\
            qs += Djm*s_Iq[k][m][i];\
            MFEM_EXCLUSIVE_GET(r_qt) += Dkm*s_Iq[m][j][i];\
         }\
         const double qt = MFEM_EXCLUSIVE_GET(r_qt);\
         const int q = i + ((j*Q1D) + (k*Q1D*Q1D));\
         const double G00 = d(q,0,e);\
         const double G01 = d(q,1,e);\
         const double G02 = d(q,2,e);\
         const double G11 = d(q,3,e);\
         const double G12 = d(q,4,e);\
         const double G22 = d(q,5,e);\
         s_Gqr[j][i] = (G00*qr + G01*qs + G02*qt); \
s_Gqs[j][i] = (G01*qr + G11*qs + G12*qt); \
MFEM_EXCLUSIVE_GET(r_qt) = G02*qr + G12*qs + G22*qt; \
MFEM_EXCLUSIVE_INC; \
}\
}\
MFEM_SYNC_THREAD; \
MFEM_FOREACH_THREAD(j,y,Q1D)\
{\
MFEM_FOREACH_THREAD(i,x,Q1D)\
{   \
   double Aqtmp = 0; \
   for (int m = 0; m < Q1D; m++)\
   {      \
      double Dmi = s_D[m][i]; \
      double Dmj = s_D[m][j]; \
      double Dkm = s_D[k][m]; \
      Aqtmp += Dmi*s_Gqr[j][m]; \
      Aqtmp += Dmj*s_Gqs[m][i]; \
      MFEM_EXCLUSIVE_GET(r_Aq)[m] += Dkm*MFEM_EXCLUSIVE_GET(r_qt); \
   }\
   MFEM_EXCLUSIVE_GET(r_Aq)[k] += Aqtmp; \
   MFEM_EXCLUSIVE_INC; \
}\
}\
MFEM_SYNC_THREAD; \
}

MFEM_JIT
template<int T_D1D = 0, int T_Q1D = 0>
void BP3Global_v0(const int NE,
                  const Array<double> &b_,
                  const Array<double> &g_,
                  const Vector &d_,
                  const Vector &x_,
                  Vector &y_,
                  const int d1d = 0,
                  const int q1d = 0)
{

   const int D1D = T_D1D ? T_D1D : 1;
   const int Q1D = T_Q1D ? T_Q1D : 1;
   constexpr int MQ1 = T_Q1D ? T_Q1D : 1;
   constexpr int MD1 = T_D1D ? T_D1D : 1;
   MFEM_VERIFY(D1D <= MD1, "");
   MFEM_VERIFY(Q1D <= MQ1, "");

   auto b = Reshape(b_.Read(), Q1D, D1D);
   auto g = Reshape(g_.Read(), Q1D, Q1D);
   auto d = Reshape(d_.Read(), Q1D*Q1D*Q1D, 6, NE);
   auto x = Reshape(x_.Read(), D1D, D1D, D1D, NE);
   auto y = Reshape(y_.ReadWrite(), D1D, D1D, D1D, NE);

   MFEM_FORALL_3D(e, NE, Q1D, Q1D, 1,
   {
      const int D1D = T_D1D ? T_D1D : 1;
      const int Q1D = T_Q1D ? T_Q1D : 1;

      MFEM_SHARED double s_Iq[Q1D][Q1D][Q1D];
      MFEM_SHARED double s_D[Q1D][Q1D];
      MFEM_SHARED double s_I[Q1D][D1D];
      MFEM_SHARED double s_Gqr[Q1D][Q1D];
      MFEM_SHARED double s_Gqs[Q1D][Q1D];

      double MFEM_EXCLUSIVE(r_qt);
      double MFEM_EXCLUSIVE(r_q)[Q1D];
      double MFEM_EXCLUSIVE(r_Aq)[Q1D];

      MFEM_FOREACH_THREAD(j,y,Q1D)
      {
         MFEM_FOREACH_THREAD(i,x,Q1D)
         {
            s_D[j][i] = g(i,j);
            if (i<D1D) { s_I[j][i] = b(j,i); }
            if (i<D1D && j<D1D)
            {
               for (int k = 0; k < D1D; k++)
               {
                  //const int id = E*D3D + k*D1D*D1D + j*D1D + i;
                  //int localId = localizedIds[id]-1;
                  //r_q[k] = q[id];
                  MFEM_EXCLUSIVE_GET(r_q)[k] = x(i,j,k,e);
               }
            }
            MFEM_EXCLUSIVE_INC;
         }
      }
      MFEM_SYNC_THREAD;

      MFEM_FOREACH_THREAD(b,y,Q1D)
      {
         MFEM_FOREACH_THREAD(a,x,Q1D)
         {
            if (a<D1D && b<D1D)
            {
               for (int k=0; k<Q1D; ++k)
               {
                  double res = 0;
                  for (int c=0; c<D1D; ++c)
                  {
                     res += s_I[k][c]*MFEM_EXCLUSIVE_GET(r_q)[c];
                  }
                  s_Iq[k][b][a] = res;
               }
            }
            MFEM_EXCLUSIVE_INC;
         }
      }
      MFEM_SYNC_THREAD;

      MFEM_FOREACH_THREAD(k,y,Q1D)
      {
         MFEM_FOREACH_THREAD(a,x,Q1D)
         {
            if (a<D1D)
            {
               for (int b=0; b<D1D; ++b)
               {
                  MFEM_EXCLUSIVE_GET(r_Aq)[b] = s_Iq[k][b][a];
               }
               for (int j=0; j<Q1D; ++j)
               {
                  double res = 0;
                  for (int b=0; b<D1D; ++b)
                  {
                     res += s_I[j][b]*MFEM_EXCLUSIVE_GET(r_Aq)[b];
                  }
                  s_Iq[k][j][a] = res;
               }
            }
            MFEM_EXCLUSIVE_INC;
         }
      }
      MFEM_SYNC_THREAD;

      MFEM_FOREACH_THREAD(k,y,Q1D)
      {
         MFEM_FOREACH_THREAD(j,x,Q1D)
         {
            for (int a=0; a<D1D; ++a)
            {
               MFEM_EXCLUSIVE_GET(r_Aq)[a] = s_Iq[k][j][a];
            }
            for (int i=0; i<Q1D; ++i)
            {
               double res = 0;
               for (int a=0; a<D1D; ++a)
               {
                  res += s_I[i][a]*MFEM_EXCLUSIVE_GET(r_Aq)[a];
               }
               s_Iq[k][j][i] = res;
            }
            MFEM_EXCLUSIVE_INC;
         }
      }
      MFEM_SYNC_THREAD;


      MFEM_FOREACH_THREAD(j,y,Q1D)
      {
         MFEM_FOREACH_THREAD(i,x,Q1D)
         {
            for (int k = 0; k < Q1D; k++)
            {
               MFEM_EXCLUSIVE_GET(r_Aq)[k] = 0.0;
            }
            MFEM_EXCLUSIVE_INC;
         }
      }
      MFEM_SYNC_THREAD;

      // Layer by layer
      /*
            for_Q(0);
            for_Q(1);
            for_Q(2);
            for_Q(3);
            for_Q(4);
            for_Q(5);
                       */
      _Pragma("unroll Q1D")
      for (int k = 0; k < Q1D; k++)
      {
         MFEM_SYNC_THREAD;
         MFEM_FOREACH_THREAD(j,y,Q1D)
         {
            MFEM_FOREACH_THREAD(i,x,Q1D)
            {
               // share u(:,:,k)
               double qr = 0.0, qs = 0.0;
               MFEM_EXCLUSIVE_GET(r_qt) = 0.0;
               _Pragma("unroll Q1D")
               for (int m = 0; m < Q1D; m++)
               {
                  double Dim = s_D[i][m];
                  double Djm = s_D[j][m];
                  double Dkm = s_D[k][m];
                  qr += Dim*s_Iq[k][j][m];
                  qs += Djm*s_Iq[k][m][i];
                  MFEM_EXCLUSIVE_GET(r_qt) += Dkm*s_Iq[m][j][i];
               }
               const double qt = MFEM_EXCLUSIVE_GET(r_qt);
               const int q = i + ((j*Q1D) + (k*Q1D*Q1D));
               const double G00 = d(q,0,e);
               const double G01 = d(q,1,e);
               const double G02 = d(q,2,e);
               const double G11 = d(q,3,e);
               const double G12 = d(q,4,e);
               const double G22 = d(q,5,e);
               s_Gqr[j][i] = (G00*qr + G01*qs + G02*qt);
               s_Gqs[j][i] = (G01*qr + G11*qs + G12*qt);
               MFEM_EXCLUSIVE_GET(r_qt) = G02*qr + G12*qs + G22*qt;
               MFEM_EXCLUSIVE_INC;
            }
         }
         MFEM_SYNC_THREAD;

         MFEM_FOREACH_THREAD(j,y,Q1D)
         {
            MFEM_FOREACH_THREAD(i,x,Q1D)
            {
               double Aqtmp = 0;
               _Pragma("unroll Q1D")
               for (int m = 0; m < Q1D; m++)
               {
                  double Dmi = s_D[m][i];
                  double Dmj = s_D[m][j];
                  double Dkm = s_D[k][m];
                  Aqtmp += Dmi*s_Gqr[j][m];
                  Aqtmp += Dmj*s_Gqs[m][i];
                  MFEM_EXCLUSIVE_GET(r_Aq)[m] += Dkm*MFEM_EXCLUSIVE_GET(r_qt);
               }
               MFEM_EXCLUSIVE_GET(r_Aq)[k] += Aqtmp;
               MFEM_EXCLUSIVE_INC;
            }
         }
         MFEM_SYNC_THREAD;
      }

      MFEM_FOREACH_THREAD(j,y,Q1D)
      {
         MFEM_FOREACH_THREAD(i,x,Q1D)
         {
            for (int c=0; c<D1D; ++c)
            {
               double res = 0;
               for (int k=0; k<Q1D; ++k)
               {
                  res += s_I[k][c]*MFEM_EXCLUSIVE_GET(r_Aq)[k];
               }
               s_Iq[c][j][i] = res;
            }
            MFEM_EXCLUSIVE_INC;
         }
      }
      MFEM_SYNC_THREAD;

      MFEM_FOREACH_THREAD(c,y,Q1D)
      {
         MFEM_FOREACH_THREAD(i,x,Q1D)
         {
            if (c<D1D)
            {
               for (int j=0; j<Q1D; ++j)
               {
                  MFEM_EXCLUSIVE_GET(r_Aq)[j] = s_Iq[c][j][i];
               }
               for (int b=0; b<D1D; ++b)
               {
                  double res = 0;
                  for (int j=0; j<Q1D; ++j)
                  {
                     res += s_I[j][b]*MFEM_EXCLUSIVE_GET(r_Aq)[j];
                  }

                  s_Iq[c][b][i] = res;
               }
            }
            MFEM_EXCLUSIVE_INC;
         }
      }
      MFEM_SYNC_THREAD;

      MFEM_FOREACH_THREAD(c,y,Q1D)
      {
         MFEM_FOREACH_THREAD(b,x,Q1D)
         {
            if (b<D1D && c<D1D)
            {
               for (int i=0; i<Q1D; ++i)
               {
                  MFEM_EXCLUSIVE_GET(r_Aq)[i] = s_Iq[c][b][i];
               }
               for (int a=0; a<D1D; ++a)
               {
                  double res = 0;
                  for (int i=0; i<Q1D; ++i)
                  {
                     res += s_I[i][a]*MFEM_EXCLUSIVE_GET(r_Aq)[i];
                  }
                  s_Iq[c][b][a] = res;
               }
            }
            MFEM_EXCLUSIVE_INC;
         }
      }
      MFEM_SYNC_THREAD;

      MFEM_FOREACH_THREAD(j,y,Q1D)
      {
         MFEM_FOREACH_THREAD(i,x,Q1D)
         {
            if (i<D1D && j<D1D)
            {
               _Pragma("unroll D1D")
               for (int k = 0; k < D1D; k++)
               {
                  //const int id = element*D3D +k*D1D*D1D+ j*D1D + i;
                  //int localId = localizedIds[id]-1;
                  double res = s_Iq[k][j][i];
                  //atomicAdd(Aq+localId, res); // atomic assumes Aq zerod
                  y(i,j,k,e) = res;
               }
            }
         }
      }
   });
}

//******************************************************************************
static int CeedHouseholderReflect(double *A, const double *v,
                                  const double b, const int m, const int n,
                                  const int row, const int col)
{
   for (int j=0; j<n; j++)
   {
      double w = A[0*row + j*col];
      for (int i=1; i<m; i++) { w += v[i] * A[i*row + j*col]; }
      A[0*row + j*col] -= b * w;
      for (int i=1; i<m; i++) { A[i*row + j*col] -= b * w * v[i]; }
   }
   return 0;
}

//******************************************************************************
static int CeedHouseholderApplyQ(double *A, const double *Q,
                                 const double *tau, bool tmode,
                                 int m, int n, int k,
                                 int row, int col)
{
   double v[m];
   for (int ii=0; ii<k; ii++)
   {
      int i = tmode ? ii : k-1-ii;
      for (int j=i+1; j<m; j++)
      {
         v[j] = Q[j*k+i];
      }
      // Apply Householder reflector (I - tau v v^T) coG^T
      CeedHouseholderReflect(&A[i*row], &v[i], tau[i], m-i, n, row, col);
   }
   return 0;
}

//******************************************************************************
static int CeedQRFactorization(double *mat, double *tau, int m, int n)
{
   double v[m];
   // Check m >= n
   if (n > m)
   {
      MFEM_VERIFY(false,"");
   }

   for (int i=0; i<n; i++)
   {
      // Calculate Householder vector, magnitude
      double sigma = 0.0;
      v[i] = mat[i+n*i];
      for (int j=i+1; j<m; j++)
      {
         v[j] = mat[i+n*j];
         sigma += v[j] * v[j];
      }
      double norm = sqrt(v[i]*v[i] + sigma); // norm of v[i:m]
      double Rii = -copysign(norm, v[i]);
      v[i] -= Rii;
      // norm of v[i:m] after modification above and scaling below
      //   norm = sqrt(v[i]*v[i] + sigma) / v[i];
      //   tau = 2 / (norm*norm)
      tau[i] = 2 * v[i]*v[i] / (v[i]*v[i] + sigma);
      for (int j=i+1; j<m; j++) { v[j] /= v[i]; }

      // Apply Householder reflector to lower right panel
      CeedHouseholderReflect(&mat[i*n+i+1], &v[i], tau[i], m-i, n-i-1, n, 1);
      // Save v
      mat[i+n*i] = Rii;
      for (int j=i+1; j<m; j++)
      {
         mat[i+n*j] = v[j];
      }
   }
   return 0;
}

//******************************************************************************
static int CeedBasisGetCollocatedGrad(const int P1d,
                                      const int Q1d,
                                      const Array<double> &B,
                                      const Array<double> &G,
                                      Array<double> &colograd1d)
{
   int i, j, k;
   double tau[Q1d];
   Array<double> interp1d(Q1d*P1d);
   Array<double> grad1d(Q1d*P1d);
   interp1d.HostReadWrite();
   grad1d.HostReadWrite();
   for (int d=0; d<P1d; d++)
   {
      for (int q=0; q<Q1d; q++)
      {
         interp1d[d + P1d*q] = B[q + Q1d*d];
         grad1d[d + P1d*q] = G[q + Q1d*d];
      }
   }
   CeedQRFactorization(interp1d, tau, Q1d, P1d);
   // Apply Rinv, colograd1d = grad1d Rinv
   for (i=0; i<Q1d; i++)   // Row   i
   {
      colograd1d[Q1d*i] = grad1d[P1d*i]/interp1d[0];
      for (j=1; j<P1d; j++)   // Column j
      {
         colograd1d[j+Q1d*i] = grad1d[j+P1d*i];
         for (k=0; k<j; k++)
         {
            colograd1d[j+Q1d*i] -= interp1d[j+P1d*k]*colograd1d[k+Q1d*i];
         }
         colograd1d[j+Q1d*i] /= interp1d[j+P1d*j];
      }
      for (j=P1d; j<Q1d; j++)
      {
         colograd1d[j+Q1d*i] = 0;
      }
   }
   // Apply Qtranspose, colograd = colograd Qtranspose
   CeedHouseholderApplyQ(colograd1d, interp1d, tau, false, Q1d, Q1d, P1d, 1, Q1d);
   return 0;
}

static void PADiffusionApply(const int dim,
                             const int D1D,
                             const int Q1D,
                             const int NE,
                             const Array<double> &B,
                             const Array<double> &G,
                             const Array<double> &Bt,
                             const Array<double> &Gt,
                             const Vector &op,
                             const Vector &x,
                             Vector &y)
{
#ifdef MFEM_USE_OCCA
   if (DeviceCanUseOcca())
   {
      if (dim == 2)
      {
         OccaPADiffusionApply2D(D1D, Q1D, NE, B, G, Bt, Gt, op, x, y);
         return;
      }
      if (dim == 3)
      {
         OccaPADiffusionApply3D(D1D, Q1D, NE, B, G, Bt, Gt, op, x, y);
         return;
      }
      MFEM_ABORT("OCCA PADiffusionApply unknown kernel!");
   }
#endif // MFEM_USE_OCCA

#ifndef MFEM_USE_JIT
   static bool BP3Global = getenv("LBP");
   if (BP3Global)
   {
      Array<double> coG(Q1D*Q1D);
      coG.GetMemory().UseDevice(true);
      CeedBasisGetCollocatedGrad(D1D, Q1D, B, G, coG);
      if (dim == 3)
      {
         switch ((D1D << 4 ) | Q1D)
         {
            case 0x23: return BP3Global_v0<2,3>(NE,B,coG,op,x,y);
            case 0x34: return BP3Global_v0<3,4>(NE,B,coG,op,x,y);
            case 0x45: return BP3Global_v0<4,5>(NE,B,coG,op,x,y);
            case 0x56: return BP3Global_v0<5,6>(NE,B,coG,op,x,y);
            case 0x67: return BP3Global_v0<6,7>(NE,B,coG,op,x,y);
            case 0x78: return BP3Global_v0<7,8>(NE,B,coG,op,x,y);
            case 0x89: return BP3Global_v0<8,9>(NE,B,coG,op,x,y);
            case 0xEF: return BP3Global_v0<14,15>(NE,B,coG,op,x,y);
               // default:   return PADiffusionApply3D(NE,B,G,Bt,Gt,op,x,y,D1D,Q1D);
         }
      }
   }
   else
   {
      if (dim == 3)
      {
         switch ((D1D << 4 ) | Q1D)
         {
            case 0x23: return SmemPADiffusionApply3D<2,3>(NE,B,G,op,x,y);
            case 0x34: return SmemPADiffusionApply3D<3,4>(NE,B,G,op,x,y);
            case 0x45: return SmemPADiffusionApply3D<4,5>(NE,B,G,op,x,y);
            case 0x56: return SmemPADiffusionApply3D<5,6>(NE,B,G,op,x,y);
            case 0x67: return SmemPADiffusionApply3D<6,7>(NE,B,G,op,x,y);
            case 0x78: return SmemPADiffusionApply3D<7,8>(NE,B,G,op,x,y);
            case 0x89: return SmemPADiffusionApply3D<8,9>(NE,B,G,op,x,y);
               // default:   return PADiffusionApply3D(NE,B,G,Bt,Gt,op,x,y,D1D,Q1D);
         }
      }
   }
#else // MFEM_USE_JIT
   static bool BP3Global = getenv("LBP");
   if (BP3Global)
   {
      Array<double> coG(Q1D*Q1D);
      coG.GetMemory().UseDevice(true);
      CeedBasisGetCollocatedGrad(D1D, Q1D, B, G, coG);
      if (dim == 3)
      {
         return BP3Global_v0(NE,B,coG,op,x,y,D1D,Q1D);
      }
   }
   else
   {
      if (dim == 3)
      {
         return SmemPADiffusionApply3D(NE,B,G,op,x,y,D1D,Q1D);
      }
   }
#endif // MFEM_USE_JIT
   MFEM_ABORT("Unknown kernel.");
}

// PA Diffusion Apply kernel
void DiffusionIntegrator::AddMultPA(const Vector &x, Vector &y) const
{
   PADiffusionApply(dim, dofs1D, quad1D, ne,
                    maps->B, maps->G, maps->Bt, maps->Gt,
                    pa_data, x, y);
}

} // namespace mfem
