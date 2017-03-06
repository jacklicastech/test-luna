## tokenizer

### tokenizer.deserialize

Deserializes a token ID into a token object. This couples the token data
itself to the token object, so that when the Lua garbage collector collects
the token, the token data will also be freed. Calling `tostring` on the
token object will produce a safe, but human-readable string.

Examples:

    tokenizer = require("tokenizer")
    local token = tokenizer.deserialize(token_id)


### tokenizer.tostring

Returns a safe, human-readable form of the token.

Examples:

    tokenizer = require("tokenizer")
    local token = tokenizer.deserialize(token_id)
    print(token)


