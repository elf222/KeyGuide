// elfTypesRL/integer_math.hpp

#pragma once

constexpr int MakeFurtherFromZero(int num, int diff)
{
    if (num < 0)
    {
        return num - diff;
    }
    else
    {
        return num + diff;
    }
}

constexpr bool IsEvenBand(int value, int band_width)
{
    int period = 2 * band_width;
    int position = ((value % period) + period) % period;

    return position < band_width;
}
