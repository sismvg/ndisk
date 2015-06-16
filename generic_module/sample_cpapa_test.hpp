#ifndef ISU_SAMPLE_CPAPA_TEST_HPP
#define ISU_SAMPLE_CPAPA_TEST_HPP

#ifdef _DEBUG
#define TEST_PRV_MEMBER_ACCESS public:
#else
#define TEST_PRV_MEMBER_ACCESS private:
#endif

#endif