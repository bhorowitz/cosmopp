#include <cosmo_mpi.hpp>

#include <fstream>
#include <sstream>
#include <memory>
#include <iomanip>
#include <memory>

#include <macros.hpp>
#include <exception_handler.hpp>
#include <planck_like.hpp>
#include <mn_scanner.hpp>
#include <polychord.hpp>
#include <mcmc.hpp>
#include <markov_chain.hpp>
#include <numerics.hpp>
#include <modecode.hpp>
#include <taylor_pk.hpp>
#include <ucmh_likelihood.hpp>
#include <progress_meter.hpp>

namespace
{

class TaylorParamsUCMH : public LambdaCDMParams
{
public:
    TaylorParamsUCMH(double omBH2, double omCH2, double h, double tau, double kPivot, double NPivot, int potentialChoice, bool slowRollEnd, bool eternalInflOK, double kMin = 8e-7, double kMax = 1.2, int nPoints = 500, bool useClass = false) : LambdaCDMParams(omBH2, omCH2, h, tau, 1.0, 1.0, kPivot), useClass_(useClass)
    {
        if(useClass)
        {
            taylor_.reset(new TaylorPk(kPivot, kMin, kMax, 10));
            vParams_.resize(5);
        }
        else
        {
            ModeCode::initialize(potentialChoice, kPivot, NPivot, false, false, slowRollEnd, eternalInflOK, kMin, kMax, nPoints);
            vParams_.resize(ModeCode::getNumVParams());
        }
    }

    ~TaylorParamsUCMH()
    {
    }

    void addKValue(double k, double sMin = 0, double sMax = 1, double tMin = 0, double tMax = 1)
    {
        if(useClass_)
            taylor_->addKValue(k, sMin, sMax, tMin, tMax);
        else
            ModeCode::addKValue(k, sMin, sMax, tMin, tMax);
    }

    void setBaseParams(double omBH2, double omCH2, double h, double tau)
    {
        omBH2_ = omBH2;
        omCH2_ = omCH2;
        h_ = h;
        tau_ = tau;
    }

    bool setVParams(const std::vector<double>& vParams, double *badLike)
    {
        if(useClass_)
            return taylor_->calculate(vParams, badLike);

        return ModeCode::calculate(vParams, badLike);
    }

    virtual const Math::RealFunction& powerSpectrum() const
    {
        if(useClass_) return taylor_->getScalarPs();

        return ModeCode::getScalarPs();
    }

    virtual const Math::RealFunction& powerSpectrumTensor() const
    {
        if(useClass_) return taylor_->getTensorPs();

        return ModeCode::getTensorPs();
    }

    virtual void getAllParameters(std::vector<double>& v) const
    {
        check(useClass_ || vParams_.size() == ModeCode::getNumVParams(), "");
        v.resize(4 + vParams_.size());
        v[0] = omBH2_;
        v[1] = omCH2_;
        v[2] = h_;
        v[3] = tau_;
        for(int i = 0; i < vParams_.size(); ++i)
            v[4 + i] = vParams_[i];
    }

    virtual bool setAllParameters(const std::vector<double>& v, double *badLike)
    {
        check(useClass_ || v.size() == 4 + ModeCode::getNumVParams(), "");

        output_screen1("Param values:");
        //output_log("Param values:");
        for(int i = 0; i < v.size(); ++i)
        {
            output_screen_clean1(std::setprecision(20) << "\t" << v[i]);
            //output_log(std::setprecision(20) << "\t" << v[i]);
        }
        output_screen_clean1(std::endl);
        //output_log(std::endl);
        
        /*
        if(v[4] < 0.002)
        {
            output_log(v[4] << ' ' << v[5] << ' ' << v[6] << ' ' << v[7] << ' ' << v[8] << std::endl);
        }
        */

        setBaseParams(v[0], v[1], v[2], v[3]);

        check(useClass_ || vParams_.size() == ModeCode::getNumVParams(), "");
        for(int i = 0; i < vParams_.size(); ++i)
            vParams_[i] = v[4 + i];

        check(vParams_[0] != 0, "");

        // last param is log_10(V0 / eps), need to convert to log_10(V0)
        vParams_[4] += std::log(vParams_[0]) / std::log(10.0);

        const bool res = setVParams(vParams_, badLike);
        if(!useClass_)
        {
            output_screen1("N_piv = " << ModeCode::getNPivot() << std::endl);
        }
        output_screen1("Result = " << res << std::endl);
        return res;
    }

private:
    const bool useClass_;
    std::unique_ptr<TaylorPk> taylor_;
    std::vector<double> vParams_;
};

class CombinedLikelihood : public Math::LikelihoodFunction
{
public:
    CombinedLikelihood(PlanckLikelihood& planck, CosmologicalParams *params, bool newUCMH, bool noGamma, bool use200, bool useWeak, bool lateDec) : planck_(planck), params_(params)
    {
        if(newUCMH)
        {
            std::stringstream gammaFileName, pulsarFileName;
            gammaFileName << "data/ucmh_gamma_";
            pulsarFileName << "data/ucmh_pulsar_";

            if(useWeak)
            {
                gammaFileName << "weakened";
                pulsarFileName << "weakened";
            }
            else if(use200)
            {
                gammaFileName << "200";
                pulsarFileName << "200";
            }
            else
            {
                gammaFileName << "1000";
                pulsarFileName << "1000";
            }

            gammaFileName << ".txt";
            pulsarFileName << ".txt";
            if(!noGamma)
                gamma_.reset(new UCMHLikelihood(gammaFileName.str().c_str(), lateDec));

            pulsar_.reset(new UCMHLikelihood(pulsarFileName.str().c_str(), lateDec));
        }
    }

    virtual double calculate(double* params, int nParams)
    {
        double l = planck_.calculate(params, nParams);
        if(l <= 1e8)
        {
            if(gamma_)
            {
                const double gammaLike = gamma_->calculate(params_->powerSpectrum());
                if(gammaLike != 0)
                {
                    output_screen("NONZERO GAMMA LIKE: " << gammaLike << std::endl);
                    l += gammaLike;
                }

            }
            if(pulsar_)
            {
                const double pulsarLike = pulsar_->calculate(params_->powerSpectrum());
                if(pulsarLike != 0)
                {
                    output_screen("NONZERO PULSAR LIKE: " << pulsarLike << std::endl);
                    l += pulsarLike;
                }
            }
        }
        return l;
    }
private:
    PlanckLikelihood& planck_;
    CosmologicalParams *params_;
    std::unique_ptr<UCMHLikelihood> gamma_;
    std::unique_ptr<UCMHLikelihood> pulsar_;
};

} // namespace

int main(int argc, char *argv[])
{
    try {
        bool ucmhLim = false;
        bool useClass = false;
        bool useMH = false;
        bool usePoly = false;
        bool newUCMH = false;
        bool noGamma = false;
        bool use200 = false;
        bool useWeak = false;
        bool lateDecoupling = false;

        bool pbhLimits = false;

        for(int i = 1; i < argc; ++i)
        {
            if(std::string(argv[i]) == std::string("ucmh"))
                ucmhLim = true;

            if(std::string(argv[i]) == std::string("class"))
                useClass = true;

            if(std::string(argv[i]) == std::string("mh"))
                useMH = true;

            if(std::string(argv[i]) == std::string("poly"))
                usePoly = true;

            if(std::string(argv[i]) == std::string("new_ucmh"))
                newUCMH = true;

            if(std::string(argv[i]) == std::string("no_gamma"))
                noGamma = true;

            if(std::string(argv[i]) == std::string("ucmh_200"))
                use200 = true;

            if(std::string(argv[i]) == std::string("ucmh_weak"))
                useWeak = true;

            if(std::string(argv[i]) == std::string("ucmh_late_dec"))
                lateDecoupling = true;

            if(std::string(argv[i]) == std::string("pbh"))
                pbhLimits = true;
        }

        if(newUCMH)
            ucmhLim = false;

        if(useClass)
        {
            output_screen("Using CLASS for calculating pk." << std::endl);
        }
        else
        {
            output_screen("Using Modecode for calculating pk. To use CLASS instead specify \"class\" as an argument." << std::endl);
        }

        if(useMH)
        {
            output_screen("Using Metropolis-Hastings sampler." << std::endl);
        }
        else if(usePoly)
        {
            output_screen("Using Polychord sampler." << std::endl);
        }
        {
            output_screen("Using MultiNest sampler. To use Polychord instead specify \"poly\" as an argument. To use Metropolis-Hastings instead specify \"mh\" as an argument." << std::endl);
        }

        if(newUCMH)
        {
            output_screen("Using the new UCMH limits." << std::endl);
            if(noGamma)
            {
                output_screen("The gamma-ray ucmh limits will NOT be included." << std::endl);
            }
            else
            {
                output_screen("The gamma-ray ucmh limits are included. To not include those specify \"no_gamma\" as an argument." << std::endl);
            }

            if(useWeak)
            {
                output_screen("The weak ucmh limits will be used." << std::endl);
            }
            else if(use200)
            {
                output_screen("z_c = 200 ucmh limits will be used. To use the weak ones specify \"ucmh_weak\" as an argument instead of \"ucmh_200\"." << std::endl);
            }
            else
            {
                output_screen("z_c = 1000 ucmh limits will be used. To use the z_c = 200 instead specify \"ucmh_200\" as an argument. If you want the weak ucmh limits instead specify \"ucmh_weak\" as an argument." << std::endl);
            }

            if(lateDecoupling)
            {
                output_screen("Using LATE kinetic decoupling for ucmh." << std::endl);
            }
            else
            {
                output_screen("Using EARLY kinetic decoupling for ucmh. To use late decoupling instead specify \"ucmh_late_dec\" as an argument." << std::endl);
            }
        }
        else
        {
            output_screen("Not using the new UCMH limits. To use those specify \"new_ucmh\" as an argument." << std::endl);
        }

        std::string root;

        if(useMH)
            root = "slow_test_files/mh_ucmh";
        else if(usePoly)
            root = "slow_test_files/pc_ucmh";
        else
            root = "slow_test_files/mn_ucmh";

        std::unique_ptr<MnScanner> mn;
        std::unique_ptr<Math::MetropolisHastings> mh;
        std::unique_ptr<PolyChord> pc;

#ifdef COSMO_PLANCK_15
        PlanckLikelihood planck(true, true, true, true, true, false, false, true, 500);
        const int nPar = 10;
#else
        PlanckLikelihood planck(true, true, false, true, false, true, 500);
        const int nPar = 23;
#endif

        const double kPivot = 0.05;

        //model 1
        //const bool slowRollEnd = true;
        //const bool eternalInflOK = false;

        //model 2
        const bool slowRollEnd = false;
        const bool eternalInflOK = true;
        TaylorParamsUCMH modelParams(0.02, 0.1, 0.7, 0.1, kPivot, 55, 12, slowRollEnd, eternalInflOK, 5e-6, 1.2, 500, useClass);
        //TaylorParamsUCMH modelParams(0.02, 0.1, 0.7, 0.1, kPivot, 55, 12, slowRollEnd, eternalInflOK, 5e-6, 0.7, 500, useClass);

        if(ucmhLim)
        {
            output_screen("Adding UCMH limits!" << std::endl);
            modelParams.addKValue(10, 0, 1e-6, 0, 1e10);
            modelParams.addKValue(1e3, 0, 1e-7, 0, 1e10);
            modelParams.addKValue(1e6, 0, 1e-7, 0, 1e10);
            modelParams.addKValue(1e9, 0, 1e-2, 0, 1e10);
        }
        else
        {
            output_screen("No UCMH limits! To add these limits specify \"ucmh\" as an argument." << std::endl);
            if(newUCMH)
            {
                /*
                modelParams.addKValue(10, 0, 1.0, 0, 1e10);
                modelParams.addKValue(1e3, 0, 1.0, 0, 1e10);
                modelParams.addKValue(1e6, 0, 1.0, 0, 1e10);
                modelParams.addKValue(1e9, 0, 1e-2, 0, 1e10);
                */
            }
        }

        if(pbhLimits)
        {
            std::ifstream inPBH("data/PBH_limits.dat");
            if(!inPBH)
            {
                StandardException exc;
                std::string exceptionStr = "Cannot read the file data/PBH_limits.dat";
                exc.set(exceptionStr);
                throw exc;
            }
            while(true)
            {
                std::string s;
                std::getline(inPBH, s);
                if(s == "")
                    break;
                if(s[0] == '#')
                    continue;

                std::stringstream str(s);
                double k, lim;
                str >> k >> lim;
                lim = std::pow(10.0, lim);

                if(useClass && k > 1e9)
                    continue;

                //output_screen("PBH limit:\t" << k << '\t' << lim << std::endl);
                modelParams.addKValue(k, 0, lim, 0, 1e10);
            }
        }
        else if(newUCMH)
        {
            modelParams.addKValue(1e3, 0, 1e10, 0, 1e10);
            modelParams.addKValue(3e3, 0, 1e10, 0, 1e10);
            modelParams.addKValue(1e4, 0, 1e10, 0, 1e10);
            modelParams.addKValue(3e4, 0, 1e10, 0, 1e10);
            modelParams.addKValue(1e5, 0, 1e10, 0, 1e10);
            modelParams.addKValue(3e5, 0, 1e10, 0, 1e10);
            modelParams.addKValue(1e6, 0, 1e10, 0, 1e10);
            modelParams.addKValue(3e6, 0, 1e10, 0, 1e10);
            modelParams.addKValue(1e7, 0, 1e10, 0, 1e10);
            modelParams.addKValue(3e7, 0, 1e10, 0, 1e10);
            modelParams.addKValue(1e8, 0, 1e10, 0, 1e10);
            modelParams.addKValue(3e8, 0, 1e10, 0, 1e10);
            modelParams.addKValue(1e9, 0, 1e10, 0, 1e10);
        }

        planck.setModelCosmoParams(&modelParams);

        CombinedLikelihood like(planck, &modelParams, newUCMH, noGamma, use200, useWeak, lateDecoupling);

        if(useMH)
            mh.reset(new Math::MetropolisHastings(nPar, like, root));
        else if(usePoly)
            pc.reset(new PolyChord(nPar, like, 500, root, 8));
        else
            mn.reset(new MnScanner(nPar, like, (pbhLimits ? 2000 : 500), root));


        int nChains;
        unsigned long burnin;
        unsigned int thin;

        if(useMH)
        {
            mh->setParam(0, "ombh2", 0.02, 0.025, 0.022, 0.0003, 0.0001);
            mh->setParam(1, "omch2", 0.1, 0.2, 0.12, 0.003, 0.001);
            mh->setParam(2, "h", 0.55, 0.85, 0.68, 0.02, 0.005);
            mh->setParam(3, "tau", 0.02, 0.2, 0.1, 0.02, 0.01);
            mh->setParam(4, "v_1", 0, 0.1, 0.01, 0.005, 0.005);
            mh->setParam(5, "v_2", -0.1, 0.1, 0, 0.02, 0.02);
            mh->setParam(6, "v_3", -0.1, 0.1, 0, 0.01, 0.01);
            mh->setParam(7, "v_4", -0.1, 0.1, 0, 0.01, 0.01);
            //mh->setParam(8, "v_5", -10, -8, -9, 0.5, 0.1);
            mh->setParam(8, "v_5", -10, -4, -6, 0.5, 0.1);

#ifdef COSMO_PLANCK_15
            mh->setParamGauss(9, "A_planck", 1.0, 0.0025, 1.0, 0.002, 0.002);
#else
            mh->setParam(9, "A_ps_100", 0, 360, 100, 100, 20);
            mh->setParam(10, "A_ps_143", 0, 270, 50, 20, 2);
            mh->setParam(11, "A_ps_217", 0, 450, 100, 30, 4);
            mh->setParam(12, "A_cib_143", 0, 20, 10, 10, 1);
            mh->setParam(13, "A_cib_217", 0, 80, 30, 15, 1);
            mh->setParam(14, "A_sz", 0, 10, 5, 5, 1);
            mh->setParam(15, "r_ps", 0.0, 1.0, 0.9, 0.2, 0.02);
            mh->setParam(16, "r_cib", 0.0, 1.0, 0.4, 0.4, 0.05);
            mh->setParam(17, "n_Dl_cib", -2, 2, 0.5, 0.2, 0.02);
            mh->setParam(18, "cal_100", 0.98, 1.02, 1.0, 0.0008, 0.0001);
            mh->setParam(19, "cal_127", 0.95, 1.05, 1.0, 0.003, 0.0002);
            mh->setParam(20, "xi_sz_cib", 0, 1, 0.5, 0.6, 0.05);
            mh->setParam(21, "A_ksz", 0, 10, 5, 6, 0.5);
            mh->setParam(22, "Bm_1_1", -20, 20, 0.5, 1.0, 0.1);
#endif

            burnin = 1000;
            thin = 2;
            nChains = mh->run(100000, 1, burnin, Math::MetropolisHastings::GELMAN_RUBIN, 0.01, false);
        }
        else if(usePoly)
        {
            pc->setParam(0, "ombh2", 0.02, 0.025, 1);
            pc->setParam(1, "omch2", 0.1, 0.2, 1);
            pc->setParam(2, "h", 0.55, 0.85, 1);
            pc->setParam(3, "tau", 0.02, 0.20, 1);
            pc->setParam(4, "v_1", 0, 0.1, 2);
            pc->setParam(5, "v_2", -0.1, 0.1, 2);
            pc->setParam(6, "v_3", -0.1, 0.1, 2);
            //pc->setParamFixed(6, "v_3", 0.0, 2);
            pc->setParam(7, "v_4", -0.1, 0.1, 2);
            //pc->setParamFixed(7, "v_4", 0.0, 2);
            pc->setParam(8, "v_5", -10, -4, 2);

#ifdef COSMO_PLANCK_15
            pc->setParamGauss(9, "A_planck", 1.0, 0.0025, 3);
#else
            pc->setParam(9, "A_ps_100", 0, 360, 3);
            pc->setParam(10, "A_ps_143", 0, 270, 3);
            pc->setParam(11, "A_ps_217", 0, 450, 3);
            pc->setParam(12, "A_cib_143", 0, 20, 3);
            pc->setParam(13, "A_cib_217", 0, 80, 3);
            pc->setParam(14, "A_sz", 0, 10, 3);
            pc->setParam(15, "r_ps", 0.0, 1.0, 3);
            pc->setParam(16, "r_cib", 0.0, 1.0, 3);
            pc->setParam(17, "n_Dl_cib", -2, 2, 3);
            pc->setParam(18, "cal_100", 0.98, 1.02, 3);
            pc->setParam(19, "cal_127", 0.95, 1.05, 3);
            pc->setParam(20, "xi_sz_cib", 0, 1, 3);
            pc->setParam(21, "A_ksz", 0, 10, 3);
            pc->setParam(22, "Bm_1_1", -20, 20, 3);
#endif
            std::vector<double> fracs{0.5, 0.4, 0.1};
            pc->setParameterHierarchy(fracs);

            pc->run(true);

            nChains = 1;
            burnin = 0;
            thin = 1;
        }
        else
        {
            mn->setParam(0, "ombh2", 0.02, 0.025);
            mn->setParam(1, "omch2", 0.1, 0.2);
            mn->setParam(2, "h", 0.55, 0.85);
            mn->setParam(3, "tau", 0.02, 0.20);
            mn->setParam(4, "v_1", 0, 0.1);
            mn->setParam(5, "v_2", -0.1, 0.1);
            mn->setParam(6, "v_3", -0.1, 0.1);
            //mn->setParamFixed(6, "v_3", 0.0);
            mn->setParam(7, "v_4", -0.1, 0.1);
            //mn->setParamFixed(7, "v_4", 0.0);
            mn->setParam(8, "v_5", -10, -4);

#ifdef COSMO_PLANCK_15
            mn->setParamGauss(9, "A_planck", 1.0, 0.0025);
#else
            mn->setParam(9, "A_ps_100", 0, 360);
            mn->setParam(10, "A_ps_143", 0, 270);
            mn->setParam(11, "A_ps_217", 0, 450);
            mn->setParam(12, "A_cib_143", 0, 20);
            mn->setParam(13, "A_cib_217", 0, 80);
            mn->setParam(14, "A_sz", 0, 10);
            mn->setParam(15, "r_ps", 0.0, 1.0);
            mn->setParam(16, "r_cib", 0.0, 1.0);
            mn->setParam(17, "n_Dl_cib", -2, 2);
            mn->setParam(18, "cal_100", 0.98, 1.02);
            mn->setParam(19, "cal_127", 0.95, 1.05);
            mn->setParam(20, "xi_sz_cib", 0, 1);
            mn->setParam(21, "A_ksz", 0, 10);
            mn->setParam(22, "Bm_1_1", -20, 20);
#endif

            mn->run(true);

            nChains = 1;
            burnin = 0;
            thin = 1;
        }
        
        if(!CosmoMPI::create().isMaster())
            return 0;

        MarkovChain chain(nChains, root.c_str(), burnin, thin);

        std::vector<MarkovChain::Element*> container;
        chain.getRange(container, 1.0, 0.0);

        std::ofstream outParamLimits("slow_test_files/mn_ucmh_param_limits.txt");
        for(int i = 0; i < nPar; ++i)
        {
            std::string paramName = (useMH ? mh->getParamName(i) : mn->getParamName(i));

            std::stringstream fileName;
            fileName << "slow_test_files/mn_ucmh_" << paramName << ".txt";
            std::unique_ptr<Posterior1D> p(chain.posterior(i));
            p->writeIntoFile(fileName.str().c_str());

            const double median = p->median();
            double lower, upper;
            p->get1SigmaTwoSided(lower, upper);
            const double sigma = (upper - lower) / 2.0;

            outParamLimits << paramName << " = " << median << "+-" << sigma << std::endl;
        }
        outParamLimits.close();
    } catch (std::exception& e)
    {
        output_screen("EXCEPTION CAUGHT!!! " << std::endl << e.what() << std::endl);
        output_screen("Terminating!" << std::endl);
        return 1;
    }
    return 0;
}
