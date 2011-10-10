int mfunc(double cx, double cy, int max_iterations, double *fx, double *fy)
{
	int i = 0;
	double zr = 0.0, zi = 0.0;

	while (i < max_iterations && zr*zr + zi*zi < 2.0*2.0)
	{
		double t = zr;
		zr = zr*zr - zi*zi + cx;
		zi = 2*t*zi + cy;
		i++;
	}
	*fx = zr;
	*fy = zi;

	if (zr*zr + zi*zi < 2.0*2.0)
		return 0;

	return i;
}

