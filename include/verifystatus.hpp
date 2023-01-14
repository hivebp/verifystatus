#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>

using namespace std;
using namespace eosio;

static constexpr symbol CORE_SYMBOL = symbol("WAX", 8);

struct LABELED_LIST {
    name label;
    vector<name> collections;
};

CONTRACT verifystatus : public contract {
public:
    using contract::contract;

    ACTION init();

    ACTION vote(
        name voter,
        vector<name> upvotes,
        vector<name> downvotes
    );

    ACTION logvotes(
        name voter,
        asset vote_power,
        vector<name> upvotes,
        vector<name> downvotes
    );

    ACTION addmarket(
        name market,
        name authorized_account
    );

    ACTION remmarket(
        name market
    );

    ACTION setlist(
        name market,
        name authorized_account,
        name label,
        vector<name> collections
    );

    ACTION addtolist(
        name market,
        name authorized_account,
        name label,
        vector<name> collections
    );

    ACTION remfromlist(
        name market,
        name authorized_account,
        name label,
        vector<name> collections
    );

    ACTION checkvotes(
        vector<name> voters
    );
private:
    void check_authentication(
        name market, 
        name authorized_account
    );
    
    void check_authorized_account(
        name authorized_account
    );

    asset get_total_staked(name voter);
    
    TABLE votes_s {
        name voter;
        asset stake;
        uint64_t vote_time;
        vector<name> upvotes;
        vector<name> downvotes;

        auto primary_key() const { return voter.value; };
    };

    TABLE markets_s {
        name market;
        name authorized_account;
        vector<LABELED_LIST> lists;

        auto primary_key() const { return market.value; };
    };

    TABLE statuses_s {
        name collection;
        int64_t upvotes;
        int64_t downvotes;

        auto primary_key() const { return collection.value; };
    };

    struct collections_s {
        name             collection_name;
        name             author;
        bool             allow_notify;
        vector <name>    authorized_accounts;
        vector <name>    notify_accounts;

        auto primary_key() const { return collection_name.value; };
    };

    struct marketplaces_s {
        name marketplace_name;
        name creator;
    };

    struct userres_s {
        name                owner;
        asset               net_weight;
        asset               cpu_weight;

        auto primary_key() const { return owner.value; };
    };

    TABLE config_s {
        string version                   = "1.0.0";
        uint64_t vote_decay              = 2592000;
    };

    typedef singleton <name("config"), config_s>           config_t;
    typedef multi_index <name("config"), config_s>         config_t_for_abi;

    typedef eosio::multi_index<name("votes"), votes_s> votes_t;
    typedef eosio::multi_index<name("markets"), markets_s> markets_t;
    typedef eosio::multi_index<name("statuses"), statuses_s> statuses_t;
    typedef eosio::multi_index<name("collections"), collections_s> collections_t;
    typedef eosio::multi_index<name("marketplaces"), marketplaces_s> marketplaces_t;
    typedef eosio::multi_index<name("userres"), userres_s> userres_t;

    votes_t votes = votes_t(get_self(), get_self().value);
    markets_t markets = markets_t(get_self(), get_self().value);
    statuses_t statuses = statuses_t(get_self(), get_self().value);
    collections_t collections = collections_t(name("atomicassets"), name("atomicassets").value);
    marketplaces_t marketplaces = marketplaces_t(name("atomicmarket"), name("atomicmarket").value);
    userres_t userres = userres_t(name("eosio"), name("eosio").value);

    userres_t get_user_res(name owner) {
        return userres_t(name("eosio"), owner.value);
    }

    config_t config = config_t(get_self(), get_self().value);
};
