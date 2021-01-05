
static const float M_PI = 3.14159265f;

struct SphericalGaussian
{
	float3 amplitude;
	float3 axis;
	float sharpness;
};

float3 EvaluateSphericalGaussian(SphericalGaussian sg, float3 direction)
{
	float cosTheta = dot(sg.axis, direction);

	float3 returnVal = sg.amplitude * exp(sg.sharpness * (cosTheta - 1));

	return returnVal;
}

float3 IntegrateSphericalGaussian(SphericalGaussian sg)
{
	float3 firstFunc = (2 * M_PI * sg.amplitude) / sg.sharpness;
	float secondFunc = (1 - exp(-2 * sg.sharpness));

	return firstFunc * secondFunc;
}

SphericalGaussian EvaluateProduct(SphericalGaussian sg1, SphericalGaussian sg2)
{
	float totalSharpness = sg1.sharpness + sg2.sharpness;
	float3 finalAxis = (sg1.sharpness * sg1.axis + sg2.sharpness * sg2.axis)/(totalSharpness);
	float finalAxisAmp = length(finalAxis);

	SphericalGaussian ret;
	ret.amplitude = (sg1.amplitude * sg2.amplitude) * exp(totalSharpness * (finalAxisAmp - 1));
	ret.axis = finalAxis / finalAxisAmp;
	ret.sharpness = totalSharpness * finalAxisAmp;

	return ret;
}

float3 ApproximateSGIntegral(in SphericalGaussian sg)
{
	return 2.0 * M_PI * (sg.amplitude / sg.sharpness);
}

//-------------------------------------------------------------------------------------------------
// Returns an SG with a particular sharpness that integrates to 1
//-------------------------------------------------------------------------------------------------
SphericalGaussian MakeNormalizedSG(in float3 axis, in float sharpness)
{
	SphericalGaussian sg;
	sg.axis = axis;
	sg.sharpness = sharpness;
	sg.amplitude = 1.0f;
	sg.amplitude = rcp(ApproximateSGIntegral(sg));

	return sg;
}

float3 InnerProduct(SphericalGaussian sg1, SphericalGaussian sg2)
{
	float3 firstFunc = 2 * M_PI * sg1.amplitude * sg2.amplitude;

	float totalSharpness = sg1.sharpness + sg2.sharpness;
	float3 finalAxis = (sg1.sharpness * sg1.axis + sg2.sharpness * sg2.axis) / (totalSharpness);
	float finalAxisAmp = length(finalAxis);
	float num = exp(finalAxisAmp - totalSharpness) - exp(-finalAxisAmp - totalSharpness);
	float3 retVal = firstFunc * (num / finalAxisAmp);

	return retVal;
}

float3 SphericalGaussianIrradianceFitted(SphericalGaussian sg, float3 normal)
{
	float muDotN = dot(sg.axis, normal);
	float lambda = sg.sharpness;
	float c0 = 0.36f;
	float c1 = 1.0f / (4.0f * c0);

	float eml = exp(-lambda);
	float em2l = eml * eml;
	float rl = rcp(lambda);

	float scale = 1.0f + 2.0f * em2l - rl;
	float bias = (eml - em2l) * rl - em2l;

	float x = sqrt(1.0f - scale);
	float x0 = c0 * muDotN;
	float x1 = c1 * x;

	float n = x0 + x1;

	float y = saturate(muDotN);
	if (abs(x0) <= x1)
	{
		y = n * n / x;
	}

	float result = scale * y + bias;

	return result * ApproximateSGIntegral(sg);
}

//Structure that defines the von misses fisher distribution
struct VMF
{
    float3 mu;
    float kappa;
    float alpha;
};