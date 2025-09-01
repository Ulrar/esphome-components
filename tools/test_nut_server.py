#!/usr/bin/env python3
"""
Test script for NUT Server Component
Tests basic NUT protocol commands against ESPHome NUT server
"""

import socket
import sys
import time
import argparse


class NutClient:
    """Simple NUT protocol client for testing."""
    
    def __init__(self, host, port=3493):
        self.host = host
        self.port = port
        self.sock = None
        
    def connect(self):
        """Connect to NUT server."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        try:
            self.sock.connect((self.host, self.port))
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from server."""
        if self.sock:
            self.sock.close()
            self.sock = None
    
    def send_command(self, command):
        """Send command and get response."""
        if not self.sock:
            return None
        
        try:
            # Send command
            self.sock.send(f"{command}\n".encode())
            
            # Receive response
            response = ""
            while True:
                data = self.sock.recv(1024).decode()
                if not data:
                    break
                response += data
                
                # Check for end markers
                if "OK" in data or "ERR" in data:
                    break
                if command.startswith("LIST") and "END LIST" in data:
                    break
                if command == "HELP" and "\n" in data:
                    break
                if command in ["VERSION", "UPSDVER"] and "\n" in data:
                    break
                    
            return response.strip()
            
        except socket.timeout:
            return response.strip() if response else "TIMEOUT"
        except Exception as e:
            return f"ERROR: {e}"
    
    def login(self, username, password):
        """Authenticate with server."""
        response = self.send_command(f"LOGIN {username} {password}")
        return "OK" in response
    
    def logout(self):
        """Logout from server."""
        return self.send_command("LOGOUT")


def test_basic_commands(client):
    """Test basic NUT commands."""
    print("\n=== Testing Basic Commands ===")
    
    commands = [
        "HELP",
        "VERSION", 
        "UPSDVER",
    ]
    
    for cmd in commands:
        print(f"\n> {cmd}")
        response = client.send_command(cmd)
        print(f"< {response}")
        time.sleep(0.1)


def test_authenticated_commands(client, username, password, ups_name):
    """Test commands requiring authentication."""
    print("\n=== Testing Authenticated Commands ===")
    
    # Login
    print(f"\n> LOGIN {username} ***")
    if client.login(username, password):
        print("< OK - Login successful")
    else:
        print("< ERR - Login failed")
        return
    
    time.sleep(0.1)
    
    # Test authenticated commands
    commands = [
        "LIST UPS",
        f"LIST VAR {ups_name}",
        f"GET VAR {ups_name} battery.charge",
        f"GET VAR {ups_name} ups.status",
        f"LIST CMD {ups_name}",
        f"LIST RW {ups_name}",
    ]
    
    for cmd in commands:
        print(f"\n> {cmd}")
        response = client.send_command(cmd)
        print(f"< {response[:500]}")  # Truncate long responses
        time.sleep(0.1)
    
    # Logout
    print("\n> LOGOUT")
    response = client.logout()
    print(f"< {response}")


def test_instant_commands(client, username, password, ups_name):
    """Test instant commands."""
    print("\n=== Testing Instant Commands ===")
    
    # Login first
    print(f"\n> LOGIN {username} ***")
    if not client.login(username, password):
        print("< ERR - Login failed, skipping instant commands")
        return
    print("< OK - Login successful")
    
    # Test safe instant commands
    commands = [
        f"INSTCMD {ups_name} beeper.mute",
    ]
    
    for cmd in commands:
        print(f"\n> {cmd}")
        response = client.send_command(cmd)
        print(f"< {response}")
        time.sleep(0.5)
    
    client.logout()


def test_error_handling(client, ups_name):
    """Test error handling."""
    print("\n=== Testing Error Handling ===")
    
    # Test invalid commands
    invalid_commands = [
        "INVALID_COMMAND",
        f"GET VAR invalid_ups battery.charge",
        f"GET VAR {ups_name} invalid.variable",
        "LOGIN",  # Missing credentials
        "LIST",   # Missing subcommand
    ]
    
    for cmd in invalid_commands:
        print(f"\n> {cmd}")
        response = client.send_command(cmd)
        print(f"< {response}")
        time.sleep(0.1)


def test_concurrent_connections(host, port, num_clients=3):
    """Test multiple concurrent connections."""
    print(f"\n=== Testing {num_clients} Concurrent Connections ===")
    
    clients = []
    for i in range(num_clients):
        client = NutClient(host, port)
        if client.connect():
            print(f"Client {i+1}: Connected")
            response = client.send_command("VERSION")
            print(f"Client {i+1}: {response}")
            clients.append(client)
        else:
            print(f"Client {i+1}: Connection failed")
    
    # Clean up
    for client in clients:
        client.disconnect()
    
    print(f"All {len(clients)} clients disconnected")


def main():
    parser = argparse.ArgumentParser(description="Test NUT Server Component")
    parser.add_argument("host", help="ESP32 IP address")
    parser.add_argument("--port", type=int, default=3493, help="NUT server port")
    parser.add_argument("--username", default="nutmon", help="Username for auth")
    parser.add_argument("--password", default="nutsecret", help="Password for auth")
    parser.add_argument("--ups", default="test_ups", help="UPS name")
    parser.add_argument("--test", choices=["basic", "auth", "instant", "error", "concurrent", "all"],
                       default="all", help="Test to run")
    
    args = parser.parse_args()
    
    print(f"Testing NUT Server at {args.host}:{args.port}")
    print(f"UPS Name: {args.ups}")
    print(f"Username: {args.username}")
    print("=" * 50)
    
    # Create client
    client = NutClient(args.host, args.port)
    
    # Connect
    if not client.connect():
        print("Failed to connect to server")
        sys.exit(1)
    
    print("Connected to NUT server")
    
    try:
        # Run tests
        if args.test in ["basic", "all"]:
            test_basic_commands(client)
        
        if args.test in ["auth", "all"]:
            test_authenticated_commands(client, args.username, args.password, args.ups)
        
        if args.test in ["instant", "all"]:
            test_instant_commands(client, args.username, args.password, args.ups)
        
        if args.test in ["error", "all"]:
            test_error_handling(client, args.ups)
        
        # Disconnect for concurrent test
        client.disconnect()
        
        if args.test in ["concurrent", "all"]:
            test_concurrent_connections(args.host, args.port)
            
    except KeyboardInterrupt:
        print("\nTest interrupted")
    except Exception as e:
        print(f"Test error: {e}")
    finally:
        client.disconnect()
    
    print("\n" + "=" * 50)
    print("Test completed")


if __name__ == "__main__":
    main()