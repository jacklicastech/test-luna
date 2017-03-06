require 'test/unit'
require 'net/http'
require 'openssl'
require 'json'
require 'securerandom'

class WebApiV1Transactions < Test::Unit::TestCase
    # TODO add debug hooks to app so we can simulate user input instead of
    #      prompting for it.

    # TODO it might not be a bad idea to create a stubbed backend so we can
    #      control authorizations under test as well.

    def test_sale
        txn_id = SecureRandom.uuid
        uri = URI.parse("https://192.168.1.82:44443/v1/transactions/#{txn_id}.json")
        http = Net::HTTP.new(uri.host, uri.port)
        http.use_ssl = true
        http.verify_mode = OpenSSL::SSL::VERIFY_NONE
        # http.set_debug_output $stderr

        # initiate a transaction & validate response
        request = Net::HTTP::Post.new(uri.request_uri)
        request.content_type = "application/json"
        request.body = {amount: 100, type: "sale"}.to_json
        response = http.request(request)
        txn = JSON.parse response.body, symbolize_names: true
        assert_equal "202", response.code
        assert_equal txn_id, txn[:id]
        assert_equal "sale", txn[:type]
        assert_equal "in-progress", txn[:status]
        assert_equal 100, txn[:requested_amount]

        # initiate another transaction with same id & validate response
        request = Net::HTTP::Post.new(uri.request_uri)
        request.content_type = "application/json"
        request.body = {amount: 100, type: "sale"}.to_json
        response = http.request(request)
        assert_equal "422", response.code
        assert_nothing_raised { JSON.parse response.body }

        # poll transaction in progress & validate response
        request = Net::HTTP::Get.new(uri.request_uri)
        response = http.request(request)
        assert_equal "200", response.code
        txn = JSON.parse response.body, symbolize_names: true
        assert_equal txn_id, txn[:id]
        assert_equal "sale", txn[:type]
        assert_equal "in-progress", txn[:status]
        assert_equal 100, txn[:requested_amount]

        puts "Press enter when transaction has been completed"
        gets

        # poll completed transaction & validate response
        request = Net::HTTP::Get.new(uri.request_uri)
        response = http.request(request)
        assert_equal "200", response.code
        txn = JSON.parse response.body, symbolize_names: true
        assert_equal txn_id, txn[:id]
        assert_equal "sale", txn[:type]
        assert_equal "approved", txn[:status]
        assert_equal 100, txn[:requested_amount]

        # initiate another transaction with same id & validate response
        request = Net::HTTP::Post.new(uri.request_uri)
        request.content_type = "application/json"
        request.body = {amount: 100, type: "sale"}.to_json
        response = http.request(request)
        assert_equal "422", response.code
        assert_nothing_raised { JSON.parse response.body }

        # cleanup: flush batch to clear transaction record
        uri = URI.parse("https://192.168.1.82:44443/v1/transactions.json")
        http = Net::HTTP.new(uri.host, uri.port)
        http.use_ssl = true
        http.verify_mode = OpenSSL::SSL::VERIFY_NONE
        request = Net::HTTP::Delete.new(uri.request_uri)
        response = http.request(request)
    end

    def test_flush_empty_batch
        uri = URI.parse("https://192.168.1.82:44443/v1/transactions.json")
        http = Net::HTTP.new(uri.host, uri.port)
        http.use_ssl = true
        http.verify_mode = OpenSSL::SSL::VERIFY_NONE

        request = Net::HTTP::Delete.new(uri.request_uri)
        response = http.request(request)
        assert_equal "204", response.code
        assert response.body.nil?
    end
end

