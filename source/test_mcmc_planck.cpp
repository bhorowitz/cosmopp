#include <vector>
#include <string>
#include <sstream>

#include <test_mcmc_planck.hpp>
#include <mcmc.hpp>
#include <planck_like.hpp>
#include <markov_chain.hpp>
#include <numerics.hpp>

std::string
TestMCMCPlanck::name() const
{
    return std::string("MCMC PLANCK LIKELIHOOD TESTER");
}

unsigned int
TestMCMCPlanck::numberOfSubtests() const
{
    return 1;
}

void
TestMCMCPlanck::runSubTest(unsigned int i, double& res, double& expected, std::string& subTestName)
{
    check(i >= 0 && i < 1, "invalid index " << i);
    
    using namespace Math;

    PlanckLikelihood planckLike(true, true, true, false, false, false);
    std::string root = "slow_test_files/mcmc_planck_test";
    MetropolisHastings mh(20, planckLike, root);

    mh.setParam(0, "ombh2", 0.005, 0.1, 0.022, 0.0003);
    mh.setParam(1, "omch2", 0.001, 0.99, 0.12, 0.003);
    mh.setParam(2, "h", 0.2, 1.0, 0.7, 0.015);
    mh.setParam(3, "tau", 0.01, 0.8, 0.1, 0.03);
    mh.setParam(4, "ns", 0.9, 1.1, 1.0, 0.01);
    mh.setParam(5, "As", 2.7, 4.0, 3.0, 0.06);

    mh.setParam(6, "A_ps_100", 0, 360, 100, 60);
    mh.setParam(7, "A_ps_143", 0, 270, 50, 13);
    mh.setParam(8, "A_ps_217", 0, 450, 100, 16);
    mh.setParam(9, "A_cib_143", 0, 20, 10, 5);
    mh.setParam(10, "A_cib_217", 0, 80, 30, 7);
    mh.setParam(11, "A_sz", 0, 10, 5, 2.7);
    mh.setParam(12, "r_ps", 0.0, 1.0, 0.9, 0.08);
    mh.setParam(13, "r_cib", 0.0, 1.0, 0.4, 0.2);
    mh.setParam(14, "n_Dl_cib", -2, 2, 0.5, 0.1);
    mh.setParam(15, "cal_100", 0.98, 1.02, 1.0, 0.0004);
    mh.setParam(16, "cal_127", 0.95, 1.05, 1.0, 0.0013);
    mh.setParam(17, "xi_sz_cib", 0, 1, 0.5, 0.3);
    mh.setParam(18, "A_ksz", 0, 10, 5, 3);
    mh.setParam(19, "Bm_1_1", -20, 20, 0.5, 0.6);

    std::vector<int> blocks(1, 20);

    //mh.specifyParameterBlocks(blocks);

    const unsigned long burnin = 250;
    const int nChains = mh.run(1000, 1, burnin);
    
    subTestName = std::string("standard_param_limits");
    res = 1;
    expected = 1;

    if(!isMaster())
        return;

    MarkovChain chain(nChains, root.c_str(), burnin);

    const int nPoints = 1000;

    const double expectedMedian[6] = {0.02217, 0.1186, 0.679, 0.089, 0.9635, 3.085};
    const double expectedSigma[6] = {0.00033, 0.0031, 0.015, 0.032, 0.0094, 0.057};

    std::ofstream outParamLimits("slow_test_files/mcmc_planck_param_limits.txt");
    for(int i = 0; i < 20; ++i)
    {
        const std::string& paramName = mh.getParamName(i);
        std::stringstream fileName;
        fileName << "slow_test_files/mcmc_planck_" << paramName << ".txt";
        Posterior1D* p = chain.posterior(i);

        std::ofstream out(fileName.str().c_str());
        const double delta = (p->max() - p->min()) / nPoints;
        for(int j = 0; j <= nPoints; ++j)
        {
            double t = p->min() + j * delta;
            if(j == nPoints)
                t = p->max();
            out << t << ' ' << p->evaluate(t) << std::endl;
        }
        out.close();

        const double median = p->median();
        double lower, upper;
        p->get1SigmaTwoSided(lower, upper);
        const double sigma = (upper - lower) / 2.0;

        outParamLimits << paramName << " = " << median << "+-" << sigma << std::endl;
        // check the standard cosmological parameter limits
        if(i < 6)
        {
            if(std::abs(expectedMedian[i] - median) > expectedSigma[i] / 2)
            {
                output_screen("FAIL: Expected " << paramName << " median is " << expectedMedian[i] << ", the result is " << median << std::endl);
                res = 0;
            }

            if(!Math::areEqual(expectedSigma[i], sigma, 0.25))
            {
                output_screen("FAIL: Expected" << paramName << "sigma is " << expectedSigma[i] << ", the result is " << sigma << std::endl);
                res = 0;
            }
        }
    }
    outParamLimits.close();
}