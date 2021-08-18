//Reservoir struct
struct Reservoir
{
    float y; //The output sample
    float wsum; // the sum of weights
    float M; //the number of samples seen so far
    float W; //Probablistic weight
};

struct Sample
{
    float3 visiblePos;
    float3 visibleNormal;
    float3 samplePos;
    float3 sampleNormal;
    float3 color;
    float3 RandomNums;
};

struct GIReservoir
{
    Sample sample;
    float wsum; // the sum of weights
    float M; //the number of samples seen so far
    float W; //Probablistic weight
};

void UpdateResrvoir(inout Reservoir r, float x, float w, float rndnum)
{
    r.wsum += w;
    r.M += 1;
    if (rndnum < (w/r.wsum))
        r.y = x;

}

void UpdateGIResrvoir(inout GIReservoir r, Sample x, float w, float rndnum)
{
    r.wsum += w;
    r.M += 1;
    if (rndnum < (w / r.wsum))
        r.sample = x;

}

//Algorithm 4 of the paper
//Reservoir CombineReservoirs(Reservoir reservoirs[100], int resCount, int rndSeed)
//{
//    Reservoir s = { 0, 0, 0 };
//    
//    for (int i = 0; i < resCount; i++)
//    {
//        UpdateResrvoir(s, ,reservoirs[i].y)
//    }
//    return s;
//
//}
