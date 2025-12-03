static inline float NtcToTemperature(float value, const float *ntcCurve, int len)
{
    float result = ntcCurve[0];
    for (int i = 1; i < len; i++) {
        result = result * value + ntcCurve[i];
    }
    return result;
}

static inline float floatMaxVal(float* data, uint32_t count) {
    float result=data[0];
    for(uint32_t i = 1; i < count; i++)
        if(result < data[i])
            result = data[i];
    return result;
}

static inline float floatMinVal(float* data, uint32_t count) {
    float result=data[0];
    for(uint32_t i = 1; i < count; i++)
        if(result > data[i])
            result = data[i];
    return result;
}

static inline float floatAvgVal(float* data, uint32_t count) {
    float result=data[0];
    for(uint32_t i = 1; i < count; i++)
        result += data[i];
    return result / count;
}
