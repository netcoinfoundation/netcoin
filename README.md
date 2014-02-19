NetCoin (NET) - a fork of Litecoin that combines the random superblocks. Like Litecoin it uses scrypt as a proof of work scheme.

	- 60 seconds block time
	- Difficulty retargets every 60 blocks
	- Total coins will be 320,636,160
	- 6 confirmations for transaction
	- 50 confirmations for minted blocks
	- 1024 coins per block, and a random super block each hour that has 8X the normal payout (8096 coins)
	- The payout will be halved each 3 months (129,600 blocks)
	- To enourage the early adoption, the 1st week after the launch is a bonus week:
		- First 2 days: 5x
		- Next 2 days: 3x
		- Next 3 days: 2x

	- The default ports: Connection 11310, and RPC 11311

	- No premine


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
