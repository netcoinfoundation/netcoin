#define BOOST_TEST_MODULE Bitcoin Test Suite
#include <boost/test/unit_test.hpp>

#include "main.h"
#include "wallet.h"

#include "checkpoints.h"

CWallet* pwalletMain;
CClientUIInterface uiInterface;

// Globals normally defined in init.cpp (which is not linked into the tests)
bool fConfChange = false;
unsigned int nNodeLifespan = 7;
unsigned int nDerivationMethodIndex = 0;
unsigned int nMinerSleep = 500;
bool fUseFastIndex = false;
enum Checkpoints::CPMode CheckpointsMode = Checkpoints::STRICT;

extern bool fPrintToConsole;
extern void noui_connect();

struct TestingSetup {
    TestingSetup() {
        fPrintToConsole = true; // don't want to write to debug.log file
        noui_connect();
        pwalletMain = new CWallet();
        RegisterWallet(pwalletMain);
    }
    ~TestingSetup()
    {
        delete pwalletMain;
        pwalletMain = NULL;
    }
};

BOOST_GLOBAL_FIXTURE(TestingSetup);

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

