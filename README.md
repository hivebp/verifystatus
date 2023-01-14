# Verification Tool

This smart contract allows to register different marketplaces and push collection statuses like verified, blacklisted, whitelisted, nsfw, recommended etc into separate lists.
It thereby enables users to get the verification status of every collection on every marketplace in the ecosystem.
The contract also adds user voting to the atomicassets collections. Users vote with their EOSIO stake. They can up- and downvote collections. 
Collections owners and authorized accounts are prohibited from voting for their collections. A vote decay can be configured to remove old votes when calling a cleanup action.