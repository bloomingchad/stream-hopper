#!/bin/bash
# A helper script to interact with the Radio Browser API.
# Its ONLY job is to find a working server and fetch raw data from a given URL path.
# All logic and parsing is handled by the C++ application.

set -e # Exit immediately if a command exits with a non-zero status.

# --- Function to find a working server ---
get_api_server() {
    # Check for dependencies
    if ! command -v dig &> /dev/null || ! command -v shuf &> /dev/null; then
        echo "Error: 'dig' and 'shuf' are required. Please install dnsutils/bind-tools." >&2
        exit 1
    fi

    local servers
    # Use HTTP for now as some servers might not have valid HTTPS certs on IP addresses
    servers=$(dig +short all.api.radio-browser.info | shuf)

    if [ -z "$servers" ]; then
        echo "Error: Could not resolve any Radio Browser API servers via DNS." >&2
        return 1
    fi

    for server in $servers; do
        # Use a short timeout to quickly skip unresponsive servers
        if curl --silent --fail --max-time 3 "http://${server}/json/stats" > /dev/null; then
            echo "http://${server}"
            return 0
        fi
    done

    echo "Error: Could not find any responsive Radio Browser API servers." >&2
    return 1
}

# --- Main script logic ---
if [ -z "$1" ]; then
    echo "Usage: $0 <url_path_and_query>" >&2
    echo "Example: $0 /json/tags?order=stationcount&reverse=true" >&2
    exit 1
fi

API_SERVER=$(get_api_server)
if [ $? -ne 0 ]; then
    # Pass the error message from the function through
    echo "$API_SERVER" >&2
    exit 1
fi

URL_PATH_AND_QUERY="$1"

# Fetch the data. The C++ app is responsible for building the query string.
# -L handles redirects.
curl --silent --fail --location --user-agent "stream-hopper/1.0" "${API_SERVER}${URL_PATH_AND_QUERY}"
