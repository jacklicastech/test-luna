require 'test/unit'
require 'net/http'
require 'openssl'
require 'json'

class MissingApiTest < Test::Unit::TestCase
    def test_missing_api
    uri = URI.parse("https://192.168.1.82:44443/missing.json")
        http = Net::HTTP.new(uri.host, uri.port)
        http.use_ssl = true
        http.verify_mode = OpenSSL::SSL::VERIFY_NONE
        # http.set_debug_output $stderr

        request = Net::HTTP::Delete.new(uri.request_uri)
        response = http.request(request)
        assert_equal "404", response.code
        assert_nothing_raised { JSON.parse response.body }
    end
end
