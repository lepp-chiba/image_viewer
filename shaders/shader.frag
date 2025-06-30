#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;
uniform float u_minVal; // <--- 輝度の最小値 (0.0-1.0)
uniform float u_maxVal; // <--- 輝度の最大値 (0.0-1.0)

void main()
{
    // テクスチャから正規化されたfloat値(0.0-1.0)として輝度を読み込む
    float intensity = texture(ourTexture, TexCoord).r;

    // u_minValとu_maxValを使って、輝度範囲を0.0-1.0に引き伸ばす
    // (u_maxVal - u_minVal)が0に近い場合、0除算を避けるためにmaxで保護
    float range = max(u_maxVal - u_minVal, 0.00001);
    float normalized_intensity = (intensity - u_minVal) / range;
    
    // clampで最終的な値を0.0-1.0の範囲に収める
    normalized_intensity = clamp(normalized_intensity, 0.0, 1.0);

    FragColor = vec4(vec3(normalized_intensity), 1.0);
}
