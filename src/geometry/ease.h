#pragma once
#include <cmath>

// 圓周率常數，某些公式會用到
#ifndef PI
#define PI 3.1415926535f
#endif

class Easing
{
public:
    // ==========================================
    // 基礎線性 (無緩動)
    // ==========================================
    static float Linear(float t) { return t; }

    // ==========================================
    // Sine (正弦波) - 非常柔和，適合呼吸燈、雲朵飄動
    // ==========================================
    static float InSine(float t) { return 1.0f - std::cos((t * PI) / 2.0f); }
    static float OutSine(float t) { return std::sin((t * PI) / 2.0f); }
    static float InOutSine(float t) { return -(std::cos(PI * t) - 1.0f) / 2.0f; }

    // ==========================================
    // Quad (二次方) - 標準緩動，適合 UI、移動
    // ==========================================
    static float InQuad(float t) { return t * t; }
    static float OutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
    static float InOutQuad(float t) {
        return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }

    // ==========================================
    // Cubic (三次方) - 比 Quad 強烈一點，適合賽車、物理加速
    // ==========================================
    static float InCubic(float t) { return t * t * t; }
    static float OutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }
    static float InOutCubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    // ==========================================
    // Quart (四次方) / Quint (五次方) - 更加劇烈
    // ==========================================
    static float OutQuart(float t) { return 1.0f - std::pow(1.0f - t, 4.0f); }
    static float OutQuint(float t) { return 1.0f - std::pow(1.0f - t, 5.0f); }

    // ==========================================
    // Expo (指數) - 像閃電一樣快，極具爆發力 (適合火柱噴發)
    // ==========================================
    static float InExpo(float t) { return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f); }
    static float OutExpo(float t) { return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t); }

    // ==========================================
    // Back (回彈) - 會稍微衝過頭再縮回來 (適合 UI 彈出)
    // ==========================================
    static float InBack(float t) {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return c3 * t * t * t - c1 * t * t;
    }
    static float OutBack(float t) {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }

    // ==========================================
    // Bounce (彈跳球) - 像球掉到地上一樣彈幾下 (適合落地效果)
    // ==========================================
    static float OutBounce(float t) {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;

        if (t < 1.0f / d1) {
            return n1 * t * t;
        }
        else if (t < 2.0f / d1) {
            return n1 * (t -= 1.5f / d1) * t + 0.75f;
        }
        else if (t < 2.5f / d1) {
            return n1 * (t -= 2.25f / d1) * t + 0.9375f;
        }
        else {
            return n1 * (t -= 2.625f / d1) * t + 0.984375f;
        }
    }

    // InBounce 比較少用，通常是用 1 - OutBounce(1-t)
    static float InBounce(float t) {
        return 1.0f - OutBounce(1.0f - t);
    }

    // ==========================================
    // Elastic (橡皮筋) - 劇烈抖動 (適合受到重擊)
    // ==========================================
    static float OutElastic(float t) {
        const float c4 = (2.0f * PI) / 3.0f;
        return t == 0.0f ? 0.0f : t == 1.0f ? 1.0f : std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }
};