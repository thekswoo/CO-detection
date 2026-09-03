/*-----------------------------------------------------------
 * MLP 조기경보 추론 (FR7)
 *
 * ★ 현재는 스텁이다. 실제 추론은 9단계에서 채운다.
 *
 * 구조 (CLAUDE.md 5장):
 *   입력층(2: 정규화된 CO 농도, 정규화된 상승률)
 *     -> 은닉층(4~8, ReLU)
 *     -> 출력층(1, sigmoid) = 0~1 위험 확률
 *
 * 학습은 PC(Python)에서 하고 가중치만 Inc/mlp_weights.h에 const float 배열로
 * 하드코딩한다. 학습 시 쓴 정규화 파라미터를 여기서도 똑같이 적용해야 한다
 * (임베디드 ML에서 가장 흔한 버그).
 *
 * 스텁이 0.0을 돌려주므로 alarm_mlp는 항상 0이고, FR2의 결정론적 임계값
 * 판정은 이와 무관하게 독립적으로 동작한다 (CLAUDE.md 4.4).
 *-----------------------------------------------------------*/

#include <stdint.h>

/* co_val: 이동평균 적용된 ADC raw 값
 * rate_of_rise: ADC counts/sec
 * 반환: 0.0~1.0 위험 확률. 0.5 초과 시 조기경보. */
float mlp_infer(float co_val, float rate_of_rise)
{
    (void)co_val;
    (void)rate_of_rise;

    return 0.0f;
}
