#include <cstdint>
#include <SX1280_Regs.h>
#include <FHSS.h>
#include <options.h>
#include <unity.h>
#include <set>

// -----------------------------------------------------------------------
// Original tests (ISM_2400 domain, index 0, no exclusion zone)
// -----------------------------------------------------------------------

void test_fhss_first(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);
    TEST_ASSERT_EQUAL(FHSSgetInitialFreq(), FHSSconfig->freq_start + freq_spread * sync_channel / FREQ_SPREAD_SCALE);
}

void test_fhss_assignment(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetChannelCount();
    uint32_t initFreq = FHSSgetInitialFreq();

    uint32_t freq = initFreq;
    for (unsigned int i = 0; i < 512; i++) {
        if ((i % numFhss) == 0) {
            TEST_ASSERT_EQUAL(initFreq, freq);
        } else {
            TEST_ASSERT_NOT_EQUAL(initFreq, freq);
        }
        freq = FHSSgetNextFreq();
    }
}

void test_fhss_unique(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetChannelCount();
    std::set<uint32_t> freqs;

    for (unsigned int i = 0; i < 256; i++) {
        uint32_t freq = FHSSgetNextFreq();

        if ((i % numFhss) == 0) {
            freqs.clear();
            freqs.insert(freq);
        } else {
            bool inserted = freqs.insert(freq).second;
            TEST_ASSERT_TRUE_MESSAGE(inserted, "Should only see a frequency one time per number initial value");
        }
    }
}

void test_fhss_same(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetSequenceCount();

    uint32_t fhss[numFhss];

    for (unsigned int i = 0; i < FHSSgetSequenceCount(); i++) {
        uint32_t freq = FHSSgetNextFreq();
        fhss[i] = freq;
    }

    FHSSrandomiseFHSSsequence(0x01020304L);

    for (unsigned int i = 0; i < FHSSgetSequenceCount(); i++) {
        uint32_t freq = FHSSgetNextFreq();
        TEST_ASSERT_EQUAL(fhss[i],freq);
    }
}

void test_fhss_reg_same(void)
{
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetSequenceCount();

    uint32_t fhss[numFhss];

    for (unsigned int i = 1; i < FHSSgetSequenceCount(); i++) {
        uint32_t freq = FHSSgetNextFreq();
        uint32_t reg = FREQ_HZ_TO_REG_VAL((2400400000 + FHSSsequence[i]*1000000));
        TEST_ASSERT_UINT32_WITHIN(1, reg, freq);
    }
}

// -----------------------------------------------------------------------
// Channel-exclusion tests (TST_EXCL domain, index 1)
//
// The TST_EXCL domain mirrors the geometry of the BR_915 Brazilian domain:
//   freq_count=42 total channels, excl_start=9, excl_count=12 → 30 effective.
// Channels 9-20 fall in the "forbidden zone" and must never be visited.
// -----------------------------------------------------------------------

#define EXCL_DOMAIN_IDX 1   // index of TST_EXCL in the SX128X domains[] array

// Forbidden channel range for TST_EXCL: actual channel indices 9..20.
// These helpers return the register-value boundaries of the forbidden zone.
static uint32_t exclForbiddenLo(void)
{
    return FHSSconfig->freq_start + (9U * freq_spread / FREQ_SPREAD_SCALE);
}
static uint32_t exclForbiddenHi(void)
{
    return FHSSconfig->freq_start + (20U * freq_spread / FREQ_SPREAD_SCALE);
}

// Test: FHSSgetChannelCount() returns freq_count - excl_count (30, not 42).
void test_fhss_excl_channel_count(void)
{
    firmwareOptions.domain = EXCL_DOMAIN_IDX;
    FHSSrandomiseFHSSsequence(0x01020304L);
    TEST_ASSERT_EQUAL_UINT32(30, FHSSgetChannelCount());
}

// Test: FHSSeffToActualIdx() remaps effective indices correctly.
//   excl_start=9, excl_count=12:
//   - effective indices 0..8  → actual 0..8   (below exclusion: unchanged)
//   - effective indices 9..29 → actual 21..41  (above exclusion: +12)
void test_fhss_excl_idx_mapping(void)
{
    firmwareOptions.domain = EXCL_DOMAIN_IDX;
    FHSSrandomiseFHSSsequence(0x01020304L);

    // Below exclusion zone — index unchanged
    TEST_ASSERT_EQUAL_UINT8(0,  FHSSeffToActualIdx(0));
    TEST_ASSERT_EQUAL_UINT8(8,  FHSSeffToActualIdx(8));
    // At and above exclusion zone — shifted by excl_count (12)
    TEST_ASSERT_EQUAL_UINT8(21, FHSSeffToActualIdx(9));   // 9  + 12 = 21
    TEST_ASSERT_EQUAL_UINT8(27, FHSSeffToActualIdx(15));  // 15 + 12 = 27
    TEST_ASSERT_EQUAL_UINT8(41, FHSSeffToActualIdx(29));  // 29 + 12 = 41
}

// Test: no generated frequency ever falls inside the excluded channel range.
void test_fhss_excl_no_forbidden_freq(void)
{
    firmwareOptions.domain = EXCL_DOMAIN_IDX;
    FHSSrandomiseFHSSsequence(0x01020304L);

    uint32_t forbidden_lo = exclForbiddenLo();
    uint32_t forbidden_hi = exclForbiddenHi();

    const uint32_t numFhss = FHSSgetChannelCount();  // 30
    for (unsigned i = 0; i < numFhss * 3; i++) {
        uint32_t freq = FHSSgetNextFreq();
        TEST_ASSERT_TRUE_MESSAGE(
            freq < forbidden_lo || freq > forbidden_hi,
            "Frequency must not fall in the excluded zone");
    }
}

// Test: each of the 30 effective channels appears exactly once per FHSS cycle.
void test_fhss_excl_unique(void)
{
    firmwareOptions.domain = EXCL_DOMAIN_IDX;
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t numFhss = FHSSgetChannelCount();  // 30
    std::set<uint32_t> freqs;

    for (unsigned i = 0; i < numFhss * 2; i++) {
        uint32_t freq = FHSSgetNextFreq();
        if ((i % numFhss) == 0) {
            freqs.clear();
            freqs.insert(freq);
        } else {
            bool inserted = freqs.insert(freq).second;
            TEST_ASSERT_TRUE_MESSAGE(inserted, "Each channel should appear exactly once per cycle");
        }
    }
}

// Test: the sequence is fully reproducible when the same seed is used.
void test_fhss_excl_reproducible(void)
{
    firmwareOptions.domain = EXCL_DOMAIN_IDX;
    FHSSrandomiseFHSSsequence(0x01020304L);

    const uint32_t count = FHSSgetSequenceCount();
    uint32_t fhss[count];
    for (unsigned i = 0; i < count; i++) fhss[i] = FHSSgetNextFreq();

    FHSSrandomiseFHSSsequence(0x01020304L);  // same seed
    for (unsigned i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL(fhss[i], FHSSgetNextFreq());
    }
}

// Test: a different seed produces a different sequence (sanity check).
void test_fhss_excl_different_seed(void)
{
    firmwareOptions.domain = EXCL_DOMAIN_IDX;

    FHSSrandomiseFHSSsequence(0x01020304L);
    const uint32_t count = FHSSgetSequenceCount();
    uint32_t fhss_a[count];
    for (unsigned i = 0; i < count; i++) fhss_a[i] = FHSSgetNextFreq();

    FHSSrandomiseFHSSsequence(0xDEADBEEFL);
    uint32_t fhss_b[count];
    for (unsigned i = 0; i < count; i++) fhss_b[i] = FHSSgetNextFreq();

    bool anyDiffers = false;
    for (unsigned i = 0; i < count; i++) {
        if (fhss_a[i] != fhss_b[i]) { anyDiffers = true; break; }
    }
    TEST_ASSERT_TRUE_MESSAGE(anyDiffers, "Different seeds should produce different sequences");
}

// -----------------------------------------------------------------------
// Unity setup/teardown
// -----------------------------------------------------------------------

void setUp()
{
    // Ensure each test starts with the default domain (ISM_2400, no exclusion).
    // Tests that need the exclusion domain set it explicitly.
    firmwareOptions.domain = 0;
}

void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    // Original tests
    RUN_TEST(test_fhss_first);
    RUN_TEST(test_fhss_assignment);
    RUN_TEST(test_fhss_unique);
    RUN_TEST(test_fhss_same);
    RUN_TEST(test_fhss_reg_same);

    // Channel-exclusion tests (BR915 mechanism)
    RUN_TEST(test_fhss_excl_channel_count);
    RUN_TEST(test_fhss_excl_idx_mapping);
    RUN_TEST(test_fhss_excl_no_forbidden_freq);
    RUN_TEST(test_fhss_excl_unique);
    RUN_TEST(test_fhss_excl_reproducible);
    RUN_TEST(test_fhss_excl_different_seed);

    UNITY_END();

    return 0;
}
