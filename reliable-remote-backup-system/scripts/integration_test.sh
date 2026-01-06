#!/bin/bash
# =============================================================================
# Integration Test Script for HTTP API
# =============================================================================
# This script tests the HTTP management API endpoints.
# Prerequisites: 
#   - curl installed
#   - jq installed (for JSON parsing)
#   - Server running on localhost:8080
# =============================================================================

set -e

BASE_URL="${BASE_URL:-http://localhost:8080}"
TEST_FILE="/tmp/test_upload_$$"
PASSED=0
FAILED=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((FAILED++))
}

# Create test file
create_test_file() {
    echo "This is a test file for integration testing" > "$TEST_FILE"
    echo "Created at: $(date)" >> "$TEST_FILE"
}

# Cleanup
cleanup() {
    rm -f "$TEST_FILE"
}

trap cleanup EXIT

# =============================================================================
# Test Cases
# =============================================================================

test_health_check() {
    log_info "Testing health check endpoint..."
    
    response=$(curl -s -w "\n%{http_code}" "$BASE_URL/healthz")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "200" ]; then
        status=$(echo "$body" | jq -r '.status' 2>/dev/null || echo "")
        if [ "$status" = "ok" ]; then
            log_pass "Health check: status=$status"
        else
            log_fail "Health check: unexpected status=$status"
        fi
    else
        log_fail "Health check: HTTP $http_code"
    fi
}

test_get_files_empty() {
    log_info "Testing get files (may be empty)..."
    
    response=$(curl -s -w "\n%{http_code}" "$BASE_URL/api/files")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "200" ]; then
        success=$(echo "$body" | jq -r '.success' 2>/dev/null || echo "false")
        if [ "$success" = "true" ]; then
            count=$(echo "$body" | jq -r '.count' 2>/dev/null || echo "0")
            log_pass "Get files: count=$count"
        else
            log_fail "Get files: success=false"
        fi
    else
        log_fail "Get files: HTTP $http_code"
    fi
}

test_upload_file() {
    log_info "Testing file upload..."
    
    create_test_file
    
    response=$(curl -s -w "\n%{http_code}" \
        -F "file=@$TEST_FILE;filename=test_integration.txt" \
        "$BASE_URL/api/upload")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "200" ]; then
        success=$(echo "$body" | jq -r '.success' 2>/dev/null || echo "false")
        if [ "$success" = "true" ]; then
            filename=$(echo "$body" | jq -r '.filename' 2>/dev/null || echo "")
            size=$(echo "$body" | jq -r '.size' 2>/dev/null || echo "0")
            log_pass "Upload file: filename=$filename, size=$size"
        else
            error=$(echo "$body" | jq -r '.error' 2>/dev/null || echo "unknown")
            log_fail "Upload file: $error"
        fi
    else
        log_fail "Upload file: HTTP $http_code"
    fi
}

test_get_files_after_upload() {
    log_info "Testing get files after upload..."
    
    response=$(curl -s -w "\n%{http_code}" "$BASE_URL/api/files")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "200" ]; then
        files=$(echo "$body" | jq -r '.files[].name' 2>/dev/null || echo "")
        if echo "$files" | grep -q "test_integration.txt"; then
            log_pass "Get files: found uploaded file"
        else
            log_fail "Get files: uploaded file not found"
        fi
    else
        log_fail "Get files after upload: HTTP $http_code"
    fi
}

test_rename_file() {
    log_info "Testing file rename..."
    
    response=$(curl -s -w "\n%{http_code}" \
        -X POST \
        -H "Content-Type: application/json" \
        -d '{"oldName": "test_integration.txt", "newName": "test_renamed.txt"}' \
        "$BASE_URL/api/rename")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "200" ]; then
        success=$(echo "$body" | jq -r '.success' 2>/dev/null || echo "false")
        if [ "$success" = "true" ]; then
            log_pass "Rename file: success"
        else
            error=$(echo "$body" | jq -r '.error' 2>/dev/null || echo "unknown")
            log_fail "Rename file: $error"
        fi
    else
        log_fail "Rename file: HTTP $http_code"
    fi
}

test_delete_file() {
    log_info "Testing file delete..."
    
    response=$(curl -s -w "\n%{http_code}" \
        -X DELETE \
        "$BASE_URL/api/files/test_renamed.txt")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "200" ]; then
        success=$(echo "$body" | jq -r '.success' 2>/dev/null || echo "false")
        if [ "$success" = "true" ]; then
            log_pass "Delete file: success"
        else
            error=$(echo "$body" | jq -r '.error' 2>/dev/null || echo "unknown")
            log_fail "Delete file: $error"
        fi
    else
        log_fail "Delete file: HTTP $http_code"
    fi
}

test_delete_nonexistent() {
    log_info "Testing delete nonexistent file..."
    
    response=$(curl -s -w "\n%{http_code}" \
        -X DELETE \
        "$BASE_URL/api/files/nonexistent_file.txt")
    http_code=$(echo "$response" | tail -n1)
    
    if [ "$http_code" = "404" ]; then
        log_pass "Delete nonexistent: HTTP 404 as expected"
    else
        log_fail "Delete nonexistent: expected 404, got $http_code"
    fi
}

test_get_metrics() {
    log_info "Testing metrics endpoint..."
    
    response=$(curl -s -w "\n%{http_code}" "$BASE_URL/api/metrics")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "200" ]; then
        uptime=$(echo "$body" | jq -r '.uptime_seconds' 2>/dev/null || echo "-1")
        if [ "$uptime" != "-1" ] && [ "$uptime" != "null" ]; then
            http_total=$(echo "$body" | jq -r '.requests.http_total' 2>/dev/null || echo "0")
            log_pass "Get metrics: uptime=${uptime}s, http_requests=$http_total"
        else
            log_fail "Get metrics: invalid response format"
        fi
    else
        log_fail "Get metrics: HTTP $http_code"
    fi
}

test_invalid_filename() {
    log_info "Testing invalid filename rejection..."
    
    # Try to upload with path traversal
    response=$(curl -s -w "\n%{http_code}" \
        -F "file=@$TEST_FILE;filename=../../../etc/passwd" \
        "$BASE_URL/api/upload")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" = "400" ]; then
        log_pass "Invalid filename: rejected with 400"
    else
        success=$(echo "$body" | jq -r '.success' 2>/dev/null || echo "true")
        if [ "$success" = "false" ]; then
            log_pass "Invalid filename: rejected"
        else
            log_fail "Invalid filename: should have been rejected"
        fi
    fi
}

# =============================================================================
# Run Tests
# =============================================================================

echo "=============================================="
echo "HTTP API Integration Tests"
echo "Base URL: $BASE_URL"
echo "=============================================="

# Wait for server to be ready
log_info "Waiting for server..."
for i in {1..30}; do
    if curl -s "$BASE_URL/healthz" > /dev/null 2>&1; then
        log_info "Server is ready"
        break
    fi
    if [ $i -eq 30 ]; then
        log_fail "Server not responding after 30 seconds"
        exit 1
    fi
    sleep 1
done

# Run test cases
test_health_check
test_get_files_empty
test_upload_file
test_get_files_after_upload
test_rename_file
test_delete_file
test_delete_nonexistent
test_get_metrics
test_invalid_filename

# Summary
echo "=============================================="
echo "Test Results: ${GREEN}$PASSED passed${NC}, ${RED}$FAILED failed${NC}"
echo "=============================================="

if [ $FAILED -gt 0 ]; then
    exit 1
fi
