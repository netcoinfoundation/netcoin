
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/1a8b92aafc7d435288da2d19022080cb)](https://www.codacy.com/app/ghettomining/netcoin?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=netcoinfoundation/netcoin&amp;utm_campaign=Badge_Grade)

![badge](https://img.shields.io/discord/219586006335225856.svg?style=for-the-badge)

![alt text](http://netcoin.io/wp-content/uploads/2013/12/NETCOIN_TRANPARENT_3-INCH_4WEBno-shadow-300x257.png)

[main repo](https://github.com/netcoinfoundation/netcoin)

Before starting ensure you have the correct [dependancys (22/01/2019)](doc/dependancys.txt) installed to compile the netcoin wallet for your platform. These can be found from the links provided in the above file or below during system specific setups. Only use the links and steps outlined above if your system does not carry dependancy packages such as ubuntu and homebrew or if you are familiar with editing the makefiles/.pro to reflect your system paths and building dependencies from scratch. However if you wish to learn this process see [dependancy build instructions](doc/build-deps.txt).

Below you shall find links to the specific build environment instructions. These have been written to aid you in self compiles however you should always seek to use precompiled binaries for your system when available to ensure you are on the latest stable release intended for mainstream consumption.

[compiling headless daemon on unix platform](doc/build-unix.txt)

[compiling headless daemon on osx platform](doc/build-osx.txt)

[compiling headless daemon on windows platform](doc/build-msw.txt)

[compiling QT on multiple platforms](doc/readme-qt.rst)

Any issues/bugs please see below first to check if your bug/issue was submitted/solved before posting a new issue please try be as clear and concise about the actual issue e.g "wallet crashed" would not be enough detail to do anything and the issue will be closed. However "wallet crashed" I was trying to send coins from (address a) to (address b) both within the same wallet and the wallet reports I have sent 0 coins out but looking at the explorer (insert link with txid) it seems the coins sent but I cannot see they have moved locally, expected outcome was to see funds leave and return. recreatable : yes platform : osx other platforms affected : yes/no/untested build version : 3.0.0.0-stable result : crash/continued to run

If formatted correctly bugs shall be assigned to a member of the team for recreation and resolve. The above bug would receive status closed as an intentional bug(feature) of the wallet.

[bug/issue submission](https://github.com/netcoinfoundation/netcoin/issues)

NetCoin (NET) - Litecoin / PeerCoin POW/POSV hybrid with Personal Investment Rate

The default ports: Connection 11310, and RPC 11311 No premine.

POS Details.

POS activated on block 420,000 - Hard fork.

POSV stake once each month to earn highest possible rewards incentivises running full nodes to increase POS security.
Coin age gradually slows after 1 month with an aging half life of 90 days.

Personal Investment Rate or PIR is a special reward algorithm unique for a PoS coin. The APR Stake reward increases in line with the value of the coins staked on a sliding scale.In other words the more Netcoin you own and stake the bigger the reward you earn. The last 3 years Netcoin had 3 different levels of PIR and it reached the last and final level end 2016.

The reward rates now are as follows:

                                   NET  |  APR
                          ------------- | -------------
                                  1-999 | 2%
                              1000-9999 | 4%
                            10000-99999 | 6%
                          100000-999999 | 7%
                        1000000-9999999 | 8%
                              10000000+ | 10%

120 seconds block time POS.
1 hour minimum age for staking.
30 days max age. FAIR-WEIGHT protocol replaces the idea of 'block splitting' to prevent very large NET holders signing lots of POS blocks.

70 confirmations for minted blocks

Pow ended on block xxx last official pow block

Q4- 2018 security/chain breach occurs meaning cryptopians funds were stolen, this was an exploit in the workings between central ledgers (exchange) and the chains state all netcoins held at cryptopia were removed from their wallets and or sold into the hands of legitimate users. There is no way to tell if this exploit resulted in real transaction reversals however it is clear that any confirming stakes were wiped out aka stake sniping which is how it was picked up. Large orphan stakes. Once identified cryptopia were asked to up the confirm limits to try and stem the attack in the hopes that the network would bounce back to good standing and the attackers efforts stemmed, unfortunately this made things slightly worse as it seems the attacker had already played out this scenario and hours later attacked again. Markets were paused while we (netcoin) along with cryptopia came to terms with the damages involved. A few months investigating lead us to where we are today (22/1/2019) cryptopias ledgers are down a whopping 265,746,657 netcoins and the chain reports no changes no duplications and no more coins than the expected 10% maximum and 2% minimum inflation through proof of stake rewards, this leaves us in a sticky situation as 2 situations arise:

1. cryptopia is held accountable for all its ledgers its users keys and ultimately the loss of these coins and they have to purchase these back

2. The chain was at fault regardless of it only manipulating centrally held ledgers due to transaction malleability/rewinds/51%/other attack meaning we are responsible for ensuring users funds are replaced.


So with the above, scenario 1 and 2 are equally relevant. It was only ledgers affected which allowed the user/s to extract all funds from the site and thus we can ask that it is reimbursed however as the developers of the coin we have responsibility to ensure the safe keeping of funds also, which has meant the decision to POST MINE (sickens me to say it, an after party premine) designed to release 300,000,000 netcoins at a predefined time to a single address controlled by the foundation to later release into cryptopias custody, to allow them to let users withdraw their funds (at minimum) and allow continued trading hereafter was decided anything left over (change) shall be retained by the foundation within the same wallet address for transparency. It shall be used for staking only! Any stake rewards from this shall be divided up between chain needs (explorers, nodes,advertising,bounties and rewards). The balance shall never be allowed to fall below the amount left after reimbursement.

Every effort is being made to ensure the future success of netcoin as a cryptocurrency of the people. We thank you for your continued support these last 5 years and hope to see us all going from strength to strength onwards 2019


Development process
===================

Developers work in their own trees, then submit pull requests when
they think their feature or bug fix is ready.

The patch will be accepted if there is broad consensus that it is a
good thing.  Developers should expect to rework and resubmit patches
if they don't match the project's coding conventions (see coding.txt)
or are controversial.

The master branch is regularly built and tested, but is not guaranteed
to be completely stable. Tags are regularly created to indicate new
official, stable release versions of Netcoin coin.

Feature branches are created when there are major new features being
worked on by several people.

From time to time a pull request will become outdated. If this occurs, and
the pull is no longer automatically mergeable; a comment on the pull will
be used to issue a warning of closure. The pull will be closed 15 days
after the warning if action is not taken by the author. Pull requests closed
in this manner will have their corresponding issue labeled 'stagnant'.

Issues with no comments will be given a similar warning, and closed after
15 days from their last activity. Issues closed in this manner will be 
labeled 'stale'. 