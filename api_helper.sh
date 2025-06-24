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

    # The SRV record is the recommended way to discover servers.
    # We shuffle the list to distribute the load. `awk` extracts the server name.
    # The timeout for dig is set to 5 seconds.
    local servers
    servers=$(dig +short +time=5 SRV _api._tcp.radio-browser.info | awk '{print $4}' | shuf)

    if [ -z "$servers" ]; then
        echo "Error: Could not resolve any Radio Browser API servers via DNS." >&2
        return 1
    fi

    for server in $servers; do
        # Check if the server is reachable with a quick HEAD request (timeout 2s)
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
    if ! command -v curl &> /dev/null; then
        echo "Error: 'curl' is required. Please install it." >&2
        exit 1
    fi

    local server
    server=$(get_api_server)
    if [ $? -ne 0 ]; then
        # The error message from get_api_server has already been printed to stderr.
        exit 1
    fi

    # Fetch the raw tag data from the discovered server.
    curl --silent --fail --user-agent "stream-hopper/1.0" "$server/json/tags"
}


# --- Main script logic: argument parsing ---
if [ "$1" = "--list-tags" ]; then
    list_tags
# Future functionality like --bygenre will be added here with 'elif'.
else
    echo "Usage: $0 --list-tags" >&2
    exit 1
fi
