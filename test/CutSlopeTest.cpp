#include <gtest/gtest.h>
#include <array>
#include <string>

class CutSlopeTest : public ::testing::Test {
protected:
    static std::array<std::string, 4> generateCutSlopeStrings() {
        return {
            "12 dB/Oct",
            "24 dB/Oct",
            "36 dB/Oct",
            "48 dB/Oct"
        };
    }
};

TEST_F(CutSlopeTest, GeneratesCutSlopeStringsCorrectly) {
    auto slopes = generateCutSlopeStrings();
    
    EXPECT_EQ(slopes.size(), 4);
    EXPECT_EQ(slopes[0], "12 dB/Oct");
    EXPECT_EQ(slopes[1], "24 dB/Oct");
    EXPECT_EQ(slopes[2], "36 dB/Oct");
    EXPECT_EQ(slopes[3], "48 dB/Oct");
}

TEST_F(CutSlopeTest, ValidatesSlopeValues) {
    auto slopes = generateCutSlopeStrings();
    
    // Check that each slope is a multiple of 12
    for (size_t i = 0; i < slopes.size(); ++i) {
        int expectedValue = static_cast<int>((i + 1) * 12);
        std::string expectedString = std::to_string(expectedValue) + " dB/Oct";
        EXPECT_EQ(slopes[i], expectedString);
    }
} 