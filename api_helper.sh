#!/bin/bash

# A helper script to interact with the Radio Browser API.
# It handles server discovery and fetching raw data.
# Dependencies: curl, dig, shuf

set -e # Exit immediately if a command exits with a non-zero status.

# --- Function to find a working server ---
# It's crucial not to hardcode a single API server.
# This function gets the list of all mirrors, shuffles them,
# and returns the first one that responds.
get_api_server() {
    # Check for dependencies
    if ! command -v dig &> /dev/null || ! command -v shuf &> /dev/null; then
        echo "Error: 'dig' and 'shuf' are required. Please install dnsutils/bind-tools." >&2
        exit 1
    fi

    local servers
    servers=$(dig +short +time=5 SRV _api._tcp.radio-browser.info | awk '{print $4}' | shuf)

    if [ -z "$servers" ]; then
        echo "Error: Could not resolve any Radio Browser API servers via DNS." >&2
        return 1
    fi

    for server in $servers; do
        if curl --head --silent --fail --max-time 2 "https://$server/json/stats" > /dev/null; then
            echo "https://$server"
            return 0
        fi
    done

    echo "Error: Could not find any responsive Radio Browser API servers." >&2
    return 1
}


# --- Function to list all tags ---
# Fetches the raw JSON array of all tags from the API.
list_tags() {
    local server
    server=$(get_api_server)
    if [ $? -ne 0 ]; then exit 1; fi

    curl --silent --fail --user-agent "stream-hopper/1.0" "$server/json/tags"
}

# --- Function to fetch stations by genre ---
# Fetches stations for a given tag, sorted by votes, and formats them
# into the structure stream-hopper expects.
fetch_by_genre() {
    local genre="$1"
    if [ -z "$genre" ]; then
        echo "Error: A genre must be provided for --bygenre." >&2
        exit 1
    fi

    if ! command -v jq &> /dev/null; then
        echo "Error: 'jq' is required. Please install it to use this feature." >&2
        exit 1
    fi

    local server
    server=$(get_api_server)
    if [ $? -ne 0 ]; then exit 1; fi

    # Fetch up to 100 working stations for the tag, sorted by votes.
    # Then, use jq to filter and transform the data.
    curl --silent --fail --user-agent "stream-hopper/1.0" \
        "$server/json/stations/bytagexact/$genre?order=votes&reverse=true&hidebroken=true&limit=100" |
    jq '
        # Filter for entries with a valid url_resolved and a name that is not too long.
        map(select(.url_resolved != "" and .url_resolved != null and (.name|length) < 40)) |
        # Take the top 30 valid stations from the result.
        .[0:30] |
        # Transform each entry into the format required by stream-hopper.
        map({
            name: .name,
            urls: [.url_resolved]
        })
    '
}

# --- Main script logic: argument parsing ---
if [ -z "$1" ]; then
    echo "Usage: $0 --list-tags | --bygenre <genre>" >&2
    exit 1
fi

case "$1" in
    --list-tags)
        list_tags
        ;;
    --bygenre)
        fetch_by_genre "$2"
        ;;
    *)
        echo "Usage: $0 --list-tags | --bygenre <genre>" >&2
        exit 1
        ;;
esac
