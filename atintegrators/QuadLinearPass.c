/* QuadLinearPass.c 
   Accelerator Toolbox 
   Revision 6/27/00
   A.Terebilo terebilo@ssrl.slac.stanford.edu
*/


#include "atelem.c"
#include "atlalib.c"

struct elem {
    double Length;
    double K;
    /* Optional fields */
    double *R1;
    double *R2;
    double *T1;
    double *T2;
};

/******************************************************************************/
/* PHYSICS SECTION ************************************************************/

void quad6 (double *r, double L, double K)
{	/* K - is the quadrupole strength defined as
	   (e/Eo)(dBz/dx) [1/m^2] 
	   another notation: g0 [DESY paper]
	*/
	
    double p_norm = 1/(1+r[4]);
    double x, xpr, y ,ypr, g, t ,lt;
    double M12,M21,M34,M43,MVD,MHD;  /* non-0 elements of transfer matrix */
    
    if(K==0) /* Track as a drift */
    {	ATdrift6(r,L);
        return;
    }
    
    x   = r[0];
    xpr = r[1]*p_norm;
    y   = r[2];
    ypr = r[3]*p_norm;
    
    g  = fabs(K)/(1+r [4]);
    t  = sqrt(g);
    lt = L*t;
    
    if(K>0)
    {	/* Horizontal  */
        MHD = cos(lt);
        M12 = sin(lt)/t;
        M21 = -M12*g;
        /* Vertical */
        MVD = cosh(lt);
        M34 = sinh(lt)/t;
        M43 = M34*g;
    }
    else
    {	/* Horizontal  */
        MHD = cosh(lt);
        M12 = sinh(lt)/t;
        M21 = M12*g;
        /* Vertical */
        MVD = cos(lt);
        M34 = sin(lt)/t;
        M43 = -M34*g;
    }
    
    
    /* M transformation compute change in tau first */
    r[0]=  MHD*x + M12*xpr;
    r[1]= (M21*x + MHD*xpr)/p_norm;
    r[2]=  MVD*y + M34*ypr;
    r[3]= (M43*y + MVD*ypr)/p_norm;
    
    /* no change in r[4] (delta) */
    
    r[5]+= g*(x*x*(L-MHD*M12)-y*y*(L-MVD*M34))/4;
    r[5]+= (xpr*xpr*(L+MHD*M12)+ypr*ypr*(L+MVD*M34))/4;
    r[5]+= (x*xpr*M12*M21 + y*ypr*M34*M43)/2;
}

void QuadLinearPass(double *r, double le, double kv, double *T1, double *T2, double *R1, double *R2, int num_particles)
{	int c;
    double *r6;

    #pragma omp parallel for if (num_particles > OMP_PARTICLE_THRESHOLD) default(shared) shared(r,num_particles) private(c,r6)
    for (c = 0;c<num_particles;c++) {	r6 = r+c*6;
        if (!atIsNaN(r6[0]) && atIsFinite(r6[4])) {
            /*
		       function quad6 internally calculates the square root
			   of the energy deviation of the particle 
			   To protect against DOMAIN and OVERFLOW error, check if the
			   fifth component of the phase spacevector r6[4] is finite
             */
             /* Misalignment at entrance */
            if(T1) ATaddvv(r6,T1);
            if(R1) ATmultmv(r6,R1);

            quad6(r6,le,kv);
			
			/* Misalignment at exit */	
            if(R2) ATmultmv(r6,R2);
            if(T2) ATaddvv(r6,T2);
        }
    }
}

#if defined(MATLAB_MEX_FILE) || defined(PYAT)
ExportMode struct elem *trackFunction(const atElem *ElemData,struct elem *Elem,
        double *r_in, int num_particles, struct parameters *Param)
{
    if (!Elem) {
        double Length, K;
        double *PolynomB, *R1, *R2, *T1, *T2;
        Length=atGetDouble(ElemData,"Length"); check_error();
        /*optional fields*/
        PolynomB=atGetOptionalDoubleArray(ElemData,"PolynomB"); check_error();
        R1=atGetOptionalDoubleArray(ElemData,"R1"); check_error();
        R2=atGetOptionalDoubleArray(ElemData,"R2"); check_error();
        T1=atGetOptionalDoubleArray(ElemData,"T1"); check_error();
        T2=atGetOptionalDoubleArray(ElemData,"T2"); check_error();
        K=atGetOptionalDouble(ElemData,"K", PolynomB ? PolynomB[1] : 0.0); check_error();
        Elem = (struct elem*)atMalloc(sizeof(struct elem));
        Elem->Length=Length;
        Elem->K=K;
        /*optional fields*/
        Elem->R1=R1;
        Elem->R2=R2;
        Elem->T1=T1;
        Elem->T2=T2;
    }
    QuadLinearPass(r_in,Elem->Length,Elem->K,Elem->T1,Elem->T2,
            Elem->R1,Elem->R2,num_particles);
    return(Elem);
}

MODULE_DEF(QuadLinearPass)        /* Dummy module initialisation */

#endif /*defined(MATLAB_MEX_FILE) || defined(PYAT)*/

#if defined(MATLAB_MEX_FILE)
void mexFunction(	int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    if (nrhs == 2 ) {
        double *r_in;
        const mxArray *ElemData = prhs[0];
        int num_particles = mxGetN(prhs[1]);
        double Length, K;
        double *PolynomB, *R1, *R2, *T1, *T2;
        Length=atGetDouble(ElemData,"Length"); check_error();
        /*optional fields*/
        PolynomB=atGetOptionalDoubleArray(ElemData,"PolynomB"); check_error();
        R1=atGetOptionalDoubleArray(ElemData,"R1"); check_error();
        R2=atGetOptionalDoubleArray(ElemData,"R2"); check_error();
        T1=atGetOptionalDoubleArray(ElemData,"T1"); check_error();
        T2=atGetOptionalDoubleArray(ElemData,"T2"); check_error();
        K=atGetOptionalDouble(ElemData,"K", PolynomB ? PolynomB[1] : 0.0); check_error();
        /* ALLOCATE memory for the output array of the same size as the input  */
        plhs[0] = mxDuplicateArray(prhs[1]);
        r_in = mxGetPr(plhs[0]);
        QuadLinearPass(r_in,Length,K,T1,T2,R1,R2,num_particles);
    }
    else if (nrhs == 0) {
        /* list of required fields */
        plhs[0] = mxCreateCellMatrix(5,1);
        mxSetCell(plhs[0],0,mxCreateString("Length"));
        mxSetCell(plhs[0],4,mxCreateString("K"));
        if (nlhs>1) {
            /* list of optional fields */
            plhs[1] = mxCreateCellMatrix(8,1);
            mxSetCell(plhs[1],0,mxCreateString("T1"));
            mxSetCell(plhs[1],1,mxCreateString("T2"));
            mxSetCell(plhs[1],2,mxCreateString("R1"));
            mxSetCell(plhs[1],3,mxCreateString("R2"));
        }
    }
    else {
        mexErrMsgIdAndTxt("AT:WrongArg","Needs 0 or 2 arguments");
    }
}
#endif /* MATLAB_MEX_FILE */