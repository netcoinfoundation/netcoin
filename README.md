NetCoin (NET) - Litecoin / PeerCoin Hybrid POW/POSV hybrid with Personal Investment Rate


 - The default ports: Connection 11310, and RPC 11311
 - No premine

POW Details

 - 120 seconds block time POW
 - scrypt algorithm
 - 384 POW coins per block. Halves every 129600 blocks (3 months)
 - 8X reward random super block each hour
 - difficulty retargets on every block


POSV Details

 - POS activates on block 420,000 - Hard fork
 - Personal Investment Rate (PIR) for the APR
   Stake reward increases in line with the value of
          the coins staked on a sliding scale using the
   following thresholds

PIR Values Year 1

 NET Staked APR %
 0               10
 1,000           15
 10,000          20
 100,000         30
 1,000,000       80
 10,000,000+     100

 - 120 seconds block time POS
 - 1 hour minimum age for staking.  30 days max age.
 - FAIR-WEIGHT protocol enforced stake weight coin value cap of 100,000 NET
   replaces the idea of 'block splitting' to prevent very large NET holders
          signing lots of POS blocks

 - 6 confirmations for transaction
 - 50 confirmations for minted blocks

 - POSV - stake once each month to earn highest possible rewards
          incentivises running full nodes to increase POS security
   coin age gradually slows after 1 month with an aging half life of 90 days.
   
Articles of interest
====================
Read all about Netcoins "Fair Weight" method.

http://forum.netcoinfoundation.org/thread/719/netcoin-weight-protocol-another-clone?page=1&scrollTo=10447

Read about Netcoins "POSV" method

http://forum.netcoinfoundation.org/thread/721/posv-rewards-blockchain-guardians-netcoin
	
	
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

Issues with no commits will be given a similar warning, and closed after
15 days from their last activity. Issues closed in this manner will be 
labeled 'stale'. 
