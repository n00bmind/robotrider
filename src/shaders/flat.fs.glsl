#version 330 core

flat in uint vertColor;

out vec4 fragColor;


// TODO Move this to an include when we support that
// vec4 to rgba8 uint
// TODO Test
uint pack( vec4 value )
{
    // Ensure values are in [0..1] and make NaNs become zeros
    value = min( max( value, 0.0f ), 1.0f );
    
    // Each component gets 8 bit
    value = value * 255 + 0.5f;
    value = floor( value );
    
    // Pack into one 32 bit uint
    return( uint(value.x) |
           (uint(value.y)<< 8) |
           (uint(value.z)<<16) |
           (uint(value.w)<<24) );
}

// rgba8 uint to vec4
vec4 unpack( uint value )
{
    return vec4(float(value & 0xFFu) / 255,
                float((value >>  8) & 0xFFu) / 255,
                float((value >> 16) & 0xFFu) / 255,
                float((value >> 24) & 0xFFu) / 255);
}

void main()
{
    fragColor = unpack( vertColor );
}
