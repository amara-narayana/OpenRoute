#include "tests/test_framework.hpp"
#include "core/storage/database.hpp"

#include <filesystem>
#include <string>

using namespace openroute::storage;

TEST(DatabaseTest, OpenAndCloseInMemory) {
    Database db;
    EXPECT_TRUE(db.Open(":memory:"));
    EXPECT_TRUE(db.IsOpen());
    db.Close();
    EXPECT_FALSE(db.IsOpen());
}

TEST(DatabaseTest, MigrationsAndVersion) {
    Database db;
    EXPECT_TRUE(db.Open(":memory:"));

    EXPECT_EQ(db.GetSchemaVersion(), 0);
    EXPECT_TRUE(db.ApplyMigrations());
    EXPECT_EQ(db.GetSchemaVersion(), 2);

    // Re-applying migrations is idempotent
    EXPECT_TRUE(db.ApplyMigrations());
    EXPECT_EQ(db.GetSchemaVersion(), 2);
}

TEST(DatabaseTest, GatewayCrudOperations) {
    Database db;
    EXPECT_TRUE(db.Open(":memory:"));
    EXPECT_TRUE(db.ApplyMigrations());

    // Insert Gateway
    auto insert_stmt = db.Prepare(R"(
        INSERT INTO gateways (id, name, public_key, encrypted_private_key, listen_port, created_at, updated_at)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);
    )");

    insert_stmt.BindText(1, "gw_test_01");
    insert_stmt.BindText(2, "Home Office Gateway");
    insert_stmt.BindText(3, "dGhpcyBpcyBhIHZhbGlkIHB1YmxpYyBrZXkgYmFzZTY0Cg==");
    std::vector<uint8_t> dummy_enc = {0xAA, 0xBB, 0xCC, 0xDD};
    insert_stmt.BindBlob(4, dummy_enc.data(), dummy_enc.size());
    insert_stmt.BindInt(5, 51820);
    insert_stmt.BindInt64(6, 1700000000);
    insert_stmt.BindInt64(7, 1700000000);

    EXPECT_FALSE(insert_stmt.Step()); // Step returns false when DONE

    // Query Gateway
    auto select_stmt = db.Prepare("SELECT name, listen_port, egress_interface FROM gateways WHERE id = ?1;");
    select_stmt.BindText(1, "gw_test_01");
    EXPECT_TRUE(select_stmt.Step());

    EXPECT_EQ(select_stmt.GetText(0), "Home Office Gateway");
    EXPECT_EQ(select_stmt.GetInt(1), 51820);
    EXPECT_EQ(select_stmt.GetText(2), "Wi-Fi");
    EXPECT_FALSE(select_stmt.Step());
}

TEST(DatabaseTest, DeviceApprovalAndPolicies) {
    Database db;
    EXPECT_TRUE(db.Open(":memory:"));
    EXPECT_TRUE(db.ApplyMigrations());

    // Insert parent gateway
    db.Execute(R"(
        INSERT INTO gateways (id, name, public_key, encrypted_private_key, listen_port, created_at, updated_at)
        VALUES ('gw1', 'Test GW', 'pubkey_gw', X'010203', 51820, 1000, 1000);
    )");

    // Insert pending device
    auto dev_stmt = db.Prepare(R"(
        INSERT INTO devices (id, gateway_id, name, platform, public_key, assigned_ip, status, created_at)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);
    )");
    dev_stmt.BindText(1, "dev_pixel_9");
    dev_stmt.BindText(2, "gw1");
    dev_stmt.BindText(3, "Pixel 9 Pro");
    dev_stmt.BindText(4, "android");
    dev_stmt.BindText(5, "pubkey_pixel_9");
    dev_stmt.BindText(6, "10.77.0.2");
    dev_stmt.BindText(7, "PENDING_APPROVAL");
    dev_stmt.BindInt64(8, 1700000000);
    EXPECT_FALSE(dev_stmt.Step());

    // Update status to APPROVED
    auto update_stmt = db.Prepare("UPDATE devices SET status = 'APPROVED', approved_at = ?1 WHERE id = ?2;");
    update_stmt.BindInt64(1, 1700000050);
    update_stmt.BindText(2, "dev_pixel_9");
    EXPECT_FALSE(update_stmt.Step());

    // Verify approval
    auto check_stmt = db.Prepare("SELECT status, assigned_ip FROM devices WHERE id = ?1;");
    check_stmt.BindText(1, "dev_pixel_9");
    EXPECT_TRUE(check_stmt.Step());
    EXPECT_EQ(check_stmt.GetText(0), "APPROVED");
    EXPECT_EQ(check_stmt.GetText(1), "10.77.0.2");

    // Insert Policy with 10 Mbps bandwidth limit and 5 GB daily quota
    auto pol_stmt = db.Prepare(R"(
        INSERT INTO policies (id, device_id, max_down_bps, max_up_bps, daily_quota_bytes, allow_lan_access)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6);
    )");
    pol_stmt.BindText(1, "pol_pixel_9");
    pol_stmt.BindText(2, "dev_pixel_9");
    pol_stmt.BindInt64(3, 10'000'000); // 10 Mbps down
    pol_stmt.BindInt64(4, 5'000'000);  // 5 Mbps up
    pol_stmt.BindInt64(5, 5'000'000'000LL); // 5 GB quota
    pol_stmt.BindInt(6, 0); // Disallow LAN access
    EXPECT_FALSE(pol_stmt.Step());

    auto check_pol = db.Prepare("SELECT max_down_bps, allow_lan_access FROM policies WHERE device_id = ?1;");
    check_pol.BindText(1, "dev_pixel_9");
    EXPECT_TRUE(check_pol.Step());
    EXPECT_EQ(check_pol.GetInt64(0), 10'000'000LL);
    EXPECT_EQ(check_pol.GetInt(1), 0);
}

TEST(DatabaseTest, TransactionRollback) {
    Database db;
    EXPECT_TRUE(db.Open(":memory:"));
    EXPECT_TRUE(db.ApplyMigrations());

    db.Execute(R"(
        INSERT INTO gateways (id, name, public_key, encrypted_private_key, listen_port, created_at, updated_at)
        VALUES ('gw_tx', 'TX GW', 'pubkey_tx', X'010203', 51820, 1000, 1000);
    )");

    {
        auto tx = db.BeginTransaction();
        db.Execute(R"(
            INSERT INTO devices (id, gateway_id, name, platform, public_key, assigned_ip, status, created_at)
            VALUES ('dev_fail', 'gw_tx', 'Temporary', 'android', 'pubkey_fail', '10.77.0.99', 'PENDING_APPROVAL', 1000);
        )");
        // Scope ends without calling tx.Commit(), so automatic rollback occurs
    }

    auto check_stmt = db.Prepare("SELECT COUNT(*) FROM devices WHERE id = 'dev_fail';");
    EXPECT_TRUE(check_stmt.Step());
    EXPECT_EQ(check_stmt.GetInt(0), 0);
}

#include "core/accounting/billing_manager.hpp"

TEST(DatabaseTest, BillingManagerOperations) {
    using namespace openroute::accounting;
    Database db;
    EXPECT_TRUE(db.Open(":memory:"));
    EXPECT_TRUE(db.ApplyMigrations());

    BillingManager billing(db);
    EXPECT_TRUE(billing.Load());

    // Test default rates
    PricingRates r = billing.GetRates();
    EXPECT_EQ(r.currency, "INR");
    EXPECT_EQ(r.rate_per_gb, 50.0);

    // Update rates
    r.currency = "USD";
    r.rate_per_gb = 2.50;
    r.rate_per_hour = 1.00;
    EXPECT_TRUE(billing.UpdateRates(r));
    EXPECT_EQ(billing.GetRates().rate_per_gb, 2.50);

    // Start a call session with dial PIN "849201"
    std::string sess = billing.StartOrResumeCall("849201", "192.168.1.50", "iPhone", "Alice-Phone");
    EXPECT_FALSE(sess.empty());

    // Record 1 GB of traffic
    uint64_t one_gb = 1024ULL * 1024ULL * 1024ULL;
    EXPECT_TRUE(billing.RecordTraffic(sess, one_gb / 2, one_gb / 2));

    auto call = billing.GetCall(sess);
    EXPECT_TRUE(call.has_value());
    EXPECT_EQ(call->bytes_rx + call->bytes_tx, one_gb);
    EXPECT_GT(call->billed_amount, 2.0); // ~2.50 USD

    // Hang up
    EXPECT_TRUE(billing.HangUpCall(sess));
    auto call_after = billing.GetCall(sess);
    EXPECT_TRUE(call_after->is_hung_up);

    auto summary = billing.GetSummary();
    EXPECT_EQ(summary.currency, "USD");
    EXPECT_GT(summary.total_revenue, 2.0);
}
