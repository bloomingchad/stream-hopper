#!/bin/bash

# A helper script to interact with the Radio Browser API.
# It handles server discovery and fetching raw data.
# Dependencies: curl

set -e # Exit immediately if a command exits with a non-zero status.

# --- Function to list all tags ---
# Fetches the raw JSON array of all tags from the API.
# The C++ application will be responsible for processing this.
list_tags() {
    if ! command -v curl &> /dev/null; then
        echo "Error: 'curl' is required. Please install it to use this feature." >&2
        exit 1
    fi

    # For now, use a hardcoded, reliable server.
    # We will add dynamic server discovery in a later step.
    local server="https://de1.api.radio-browser.info"

    # Fetch the raw tag data. The C++ app will do the heavy lifting of parsing.
    # The --fail flag ensures curl exits with an error if the HTTP request fails.
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
