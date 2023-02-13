#include <verifystatus.hpp>

/**
*  Initializes the config table. Only needs to be called once when first deploying the contract
*  @required_auth The contract itself
*/
ACTION verifystatus::init() {
    require_auth(get_self());
    config.get_or_create(get_self(), config_s{});
}

asset verifystatus::get_total_staked(
    name voter
) {
    delegated_bandwidth_t resources = get_delegated_bandwidth(voter);

    auto res_ptr = resources.begin();

    asset stake = asset(0, symbol("WAX", 8));

    while (res_ptr != resources.end()) {
        stake += res_ptr->cpu_weight + res_ptr->net_weight;
        ++res_ptr;
    }

    check (stake.amount > 0, "User does not have any resources staked");

    return stake;
};

/**
*  Logs the vote action to the chain, so it can be picked up by action readers
*
*  @param voter account of the voter
*  @param vote_power the voters eosio stake
*  @param upvotes list of upvoted collections
*  @param diwbvotes list of downvoted collections
*
*  @required_auth The contract itself
*/
ACTION verifystatus::logvotes(
    name voter,
    asset vote_power,
    vector<name> upvotes,
    vector<name> downvotes
) {
    require_auth(get_self());
}

/**
*  Votes for a list of up- and downvoted collections
*
*  @param voter account of the voter
*  @param upvotes list of upvoted collections
*  @param diwbvotes list of downvoted collections
*
*  @required_auth The voter
*/
ACTION verifystatus::vote(
    name voter,
    vector<name> upvotes,
    vector<name> downvotes
) {
    require_auth(voter);

    asset total_staked = get_total_staked(voter);

    auto voter_ptr = votes.find(voter.value);

    vector<name> new_upvotes = upvotes;
    vector<name> new_downvotes = downvotes;

    for (name collection : upvotes) {
        auto collection_ptr = collections.require_find(collection.value, ("Collection " + collection.to_string() + " not found!").c_str());
        vector<name> authorized_accounts = collection_ptr->authorized_accounts;
        check (voter != collection && voter != collection_ptr->author && std::find(authorized_accounts.begin(), authorized_accounts.end(), voter) == authorized_accounts.end(), "Cannot vote for your collection");
    }

    for (name collection : downvotes) {
        auto collection_ptr = collections.require_find(collection.value, ("Collection " + collection.to_string() + " not found!").c_str());
        vector<name> authorized_accounts = collection_ptr->authorized_accounts;
        check (voter != collection && voter != collection_ptr->author && std::find(authorized_accounts.begin(), authorized_accounts.end(), voter) == authorized_accounts.end(), "Cannot vote for your collection");
    }

    if (voter_ptr == votes.end()) {
        votes.emplace(voter, [&](auto& _vote) {
            _vote.voter = voter;
            _vote.stake = total_staked;
            _vote.vote_time = current_time_point().sec_since_epoch();
            _vote.upvotes = upvotes;
            _vote.downvotes = downvotes;
        });
    } else {
        for (name collection : voter_ptr->upvotes) {
            auto status_ptr = statuses.find(collection.value);

            if (status_ptr != statuses.end()) {
                if (std::find(upvotes.begin(), upvotes.end(), collection) != upvotes.end()) {
                    new_upvotes.erase(std::find(new_upvotes.begin(), new_upvotes.end(), collection));
                    statuses.modify(status_ptr, same_payer, [&](auto &_status) {
                        _status.upvotes = _status.upvotes - voter_ptr->stake.amount + total_staked.amount;
                    });
                } else {
                    statuses.modify(status_ptr, same_payer, [&](auto &_status) {
                        _status.upvotes = _status.upvotes - voter_ptr->stake.amount;
                    });
                }
            }
        }
        for (name collection : voter_ptr->downvotes) {
            auto status_ptr = statuses.find(collection.value);

            if (status_ptr != statuses.end()) {
                if (std::find(downvotes.begin(), downvotes.end(), collection) != downvotes.end()) {
                    new_downvotes.erase(std::find(new_downvotes.begin(), new_downvotes.end(), collection));
                    statuses.modify(status_ptr, same_payer, [&](auto &_status) {
                        _status.downvotes = _status.downvotes - voter_ptr->stake.amount + total_staked.amount;
                    });
                } else {
                    statuses.modify(status_ptr, same_payer, [&](auto &_status) {
                        _status.downvotes = _status.downvotes - voter_ptr->stake.amount;
                    });
                }
            }
        }
        votes.modify(voter_ptr, voter, [&](auto& _vote) {
            _vote.upvotes = upvotes;
            _vote.downvotes = downvotes;
            _vote.stake = total_staked;
            _vote.vote_time = current_time_point().sec_since_epoch();
        });
    }

    for (name collection : new_upvotes) {
        auto status_ptr = statuses.find(collection.value);

        check(std::find(downvotes.begin(), downvotes.end(), collection) == downvotes.end(), 
            "Collection cannot be upvoted and downvoted at the same time");

        if (status_ptr == statuses.end()) {
            statuses.emplace(get_self(), [&](auto& _status) {
                _status.collection = collection;
                _status.upvotes = total_staked.amount;
                _status.downvotes = 0;
            });
        } else {
            statuses.modify(status_ptr, same_payer, [&](auto &_status) {
                _status.upvotes = _status.upvotes + total_staked.amount;
            });
        }
    }

    for (name collection : new_downvotes) {
        auto status_ptr = statuses.find(collection.value);

        check(std::find(upvotes.begin(), upvotes.end(), collection) == upvotes.end(), 
            "Collection cannot be upvoted and downvoted at the same time");

        if (status_ptr == statuses.end()) {
            statuses.emplace(get_self(), [&](auto& _status) {
                _status.collection = collection;
                _status.upvotes = 0;
                _status.downvotes = total_staked.amount;
            });
        } else {
            statuses.modify(status_ptr, same_payer, [&](auto &_status) {
                _status.downvotes = _status.downvotes + total_staked.amount;
            });
        }
    }

    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("logvotes"),
        make_tuple(
            voter,
            total_staked,
            upvotes,
            downvotes
        )
    ).send();
}

/**
*  Adds a new marketplace to aggregate collection statuses from
*
*  @param market name of the marketplace
*  @param authorized_account authorized account to change collection statuses
*
*  @required_auth The contract itself
*/
ACTION verifystatus::addmarket(
    name market,
    name authorized_account
) {
    require_auth(get_self());

    check (markets.find(market.value) == markets.end(), "Market already exists");

    markets.emplace(get_self(), [&](auto& _market) {
        _market.market = market;
        _market.authorized_account = authorized_account;
        _market.lists = {};
    });
}

/**
*  Removes a new marketplace from the contract
*
*  @param market name of the marketplace
*
*  @required_auth The contract itself
*/
ACTION verifystatus::remmarket(
    name market
) {
    require_auth(get_self());

    auto market_ptr = markets.require_find(market.value, "Market does not exist");

    markets.erase(market_ptr);
}

/**
*  Checks authorization of a marketplace
*
*  @param market name of the marketplace
*  @param authorized_account authorized account to check
*
*  @required_auth The authorized account
*/
void verifystatus::check_authentication(
    name market, 
    name authorized_account
) {
    require_auth(authorized_account);
    auto market_itr = markets.require_find(market.value, "No market with this name exists");

    check(authorized_account == get_self() || market_itr->authorized_account == authorized_account, 
        "This account is not authorized to set lists for this market");
}

void verifystatus::check_authorized_account(
    name authorized_account
) {
    require_auth(authorized_account);
}

/**
*  Adds an entire new list of a certain label to a marketplace
*
*  @param market name of the marketplace
*  @param authorized_account authorized account to change collection statuses
*  @param label name of the list to add
*  @param collections collections to add to the list
*
*  @required_auth The contract itself
*/
ACTION verifystatus::setlist(
    name market,
    name authorized_account,
    name label,
    vector<name> collections
) {
    check_authentication(market, authorized_account);

    auto market_ptr = markets.find(market.value);

    markets.modify(market_ptr, same_payer, [&](auto &_market) {
        vector<LABELED_LIST> new_lists = {};
        bool found = false;
        for (LABELED_LIST list : market_ptr->lists) {
            LABELED_LIST new_list = {};
            new_list.label = list.label;

            if (list.label == label) {
                new_list.collections = collections;
                found = true;
            } else {
                new_list.collections = list.collections;
            }

            new_lists.push_back(new_list);
        }
        if (!found) {
            LABELED_LIST new_list = {};
            new_list.label = label;
            new_list.collections = collections;
            new_lists.push_back(new_list);
        }
        _market.lists = new_lists;
    });
}

/**
*  Appends a status list with new collections
*
*  @param market name of the marketplace the list belongs to
*  @param authorized_account authorized account to change collection statuses
*  @param label name of the list to append
*  @param collections collections to add to the list
*
*  @required_auth The authorized account or the contract itself
*/
ACTION verifystatus::addtolist(
    name market,
    name authorized_account,
    name label,
    vector<name> collections
) {
    check_authentication(market, authorized_account);

    auto market_ptr = markets.find(market.value);

    markets.modify(market_ptr, same_payer, [&](auto &_market) {
        vector<LABELED_LIST> new_lists = {};
        bool found = false;
        for (LABELED_LIST list : market_ptr->lists) {
            if (list.label == label) {
                found = true;
                LABELED_LIST new_list = {};
                new_list.label = label;
                new_list.collections = {};
                for (name collection : list.collections) {
                    new_list.collections.push_back(collection);
                }
                for (name collection : collections) {
                    if (std::find(new_list.collections.begin(), new_list.collections.end(), collection) == new_list.collections.end()) {
                        new_list.collections.push_back(collection);
                    }
                }
                struct {
                    bool operator()(name a, name b) const { return a.to_string() < b.to_string(); }
                } sorter;
                std::sort(new_list.collections.begin(), new_list.collections.end(), sorter);
                new_lists.push_back(new_list);
            } else {
                new_lists.push_back(list);
            }
        }
        check(found, "List not Found");
        _market.lists = new_lists;
    });
}

/**
*  Removes collections from an existing list
*
*  @param market name of the marketplace the list belongs to
*  @param authorized_account authorized account to change collection statuses
*  @param label name of the list to remove from
*  @param collections collections to remove from the list
*
*  @required_auth The authorized account or the contract itself
*/
ACTION verifystatus::remfromlist(
    name market,
    name authorized_account,
    name label,
    vector<name> collections
) {
    check_authentication(market, authorized_account);

    auto market_ptr = markets.find(market.value);

    markets.modify(market_ptr, same_payer, [&](auto &_market) {
        vector<LABELED_LIST> new_lists = {};
        bool found = false;
        for (LABELED_LIST list : market_ptr->lists) {
            if (list.label == label) {
                found = true;
                vector<name> new_collections = {};
                LABELED_LIST new_list = {};
                new_list.label = label;

                for (name collection : list.collections) {
                    if (std::find(collections.begin(), collections.end(), collection) == collections.end()) {
                        new_collections.push_back(collection);
                    }
                }
                vector<name> A = list.collections;
                struct {
                    bool operator()(name a, name b) const { return a.to_string() < b.to_string(); }
                } sorter;
                std::sort(new_collections.begin(), new_collections.end(), sorter);
                new_list.collections = new_collections;
                if (new_collections.size() > 0) {
                    new_lists.push_back(new_list);
                }
            } else {
                new_lists.push_back(list);
            }
            
        }
        check(found, "List not Found");
        _market.lists = new_lists;
    });
}

/**
*  Checks and removes expired votes with list of voters. 
*  The list can be created externally to pre-select expired votes and remove them.
*
*  @param voters list of voters to check
*
*  @required_auth The authorizec account or the contract itself
*/
ACTION verifystatus::checkvotes(
    vector<name> voters
) {
    auto status_ptr = statuses.begin();

    config_s current_config = config.get();

    for (name voter : voters) {
        auto voter_ptr = votes.find(voter.value);

        if (voter_ptr != votes.end()) {
            asset stake = get_total_staked(voter_ptr->voter);
            if (stake.amount < voter_ptr->stake.amount || voter_ptr->vote_time < current_time_point().sec_since_epoch() - current_config.vote_decay) {
                for (name collection : voter_ptr->upvotes) {
                    auto status_ptr = statuses.find(collection.value);

                    if (status_ptr != statuses.end()) {
                        statuses.modify(status_ptr, same_payer, [&](auto &_status) {
                            _status.upvotes = _status.upvotes - voter_ptr->stake.amount;
                        });
                    }
                }
                for (name collection : voter_ptr->downvotes) {
                    auto status_ptr = statuses.find(collection.value);
                    if (status_ptr != statuses.end()) {
                        statuses.modify(status_ptr, same_payer, [&](auto &_status) {
                            _status.downvotes = _status.downvotes - voter_ptr->stake.amount;
                        });
                    }
                }
                votes.erase(voter_ptr);

                action(
                    permission_level{get_self(), name("active")},
                    get_self(),
                    name("logvotes"),
                    make_tuple(
                        voter,
                        asset(0, CORE_SYMBOL),
                        voter_ptr->upvotes,
                        voter_ptr->downvotes
                    )
                ).send();
            }
        }
    }
}

