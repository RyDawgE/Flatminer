import sys
import json
import subprocess
import copy
import os
import re

flatc_path : str = "flatc"
decoded_cache = {}  # Cache decoded results to avoid redundant flatc calls

def fix_relaxed_json(text):
    """Convert relaxed JSON (unquoted keys) to strict JSON"""
    # Add quotes around unquoted keys
    # Matches: word followed by colon
    return re.sub(r'(\s+)([a-zA-Z_][a-zA-Z0-9_]*)(\s*):', r'\1"\2"\3:', text)

def decode_nested_flatbuffer(data, temp_files=None):
    """Recursively process JSON and decode nested FlatBuffers in place"""
    if temp_files is None:
        temp_files = []
    
    if isinstance(data, dict):
        current_typename = data.get("type_name", "")
        
        # Check if this object has a nested FlatBuffer to decode
        if "nested_type" in data and current_typename and isinstance(data["nested_type"], list):
            nested_bytes = data["nested_type"]
            
            # Check cache first
            bytes_key = str(nested_bytes[:20])  # Use first 20 bytes as cache key
            if bytes_key in decoded_cache:
                data["nested_type"] = copy.deepcopy(decoded_cache[bytes_key])
            else:
                # Convert decimal array to bytes and write to temp file
                # flatc creates output JSON in same directory as input binary RELATIVE TO CWD
                abs_bin_path = os.path.abspath(f".\\build\\schemas\\{current_typename}.bin")
                abs_json_path = os.path.abspath(f".\\build\\schemas\\{current_typename}.json")
                
                # Track temp files for cleanup later
                temp_files.extend([abs_bin_path, abs_json_path])
                
                byte_data = bytes(nested_bytes)
                with open(abs_bin_path, 'wb') as bin_file:
                    bin_file.write(byte_data)
                
                # Run flatc from the schemas directory so includes work
                # Binary file is also in the schemas directory
                rel_bin_path = f"{current_typename}.bin"
                
                # Check if schema exists
                schema_path = os.path.abspath(f'.\\build\\schemas\\{current_typename}.fbs')
                if not os.path.exists(schema_path):
                    print(f"Schema not found: {schema_path}, skipping {current_typename}")
                else:
                    result = subprocess.run(
                        [flatc_path, '-t', '--raw-binary', '--strict-json', f'{current_typename}.fbs', '--', rel_bin_path], 
                        capture_output=True, text=True, cwd=os.path.abspath('.\\build\\schemas'))
                    
                    # Debug: check if binary file exists
                    if not os.path.exists(abs_bin_path):
                        print(f"ERROR: Binary file wasn't created: {abs_bin_path}")
                    
                    # If successful, read the generated JSON file
                    elif result.returncode == 0:
                        try:
                            if os.path.exists(abs_json_path):
                                with open(abs_json_path, 'r') as json_file:
                                    json_text = json_file.read()
                                # Fix relaxed JSON format (unquoted keys)
                                json_text = fix_relaxed_json(json_text)
                                decoded_data = json.loads(json_text)
                                # Cache the result
                                decoded_cache[bytes_key] = copy.deepcopy(decoded_data)
                                # Replace the byte array with the decoded JSON object
                                data["nested_type"] = decoded_data
                            else:
                                print(f"flatc didn't create output file for {current_typename}")
                                print(f"  Expected at: {abs_json_path}")
                        except (json.JSONDecodeError, IOError) as e:
                            print(f"Failed to read decoded JSON for {current_typename}: {e}")
                    else:
                        print(f"flatc error for {current_typename}:")
                        print(f"  Return code: {result.returncode}")
                        if result.stderr and "unable to load file" in result.stderr:
                            print(f"  Missing schema file")
        
        # Now recursively process ALL fields (including the now-decoded nested_type)
        for key, value in data.items():
            if isinstance(value, dict):
                decode_nested_flatbuffer(value, temp_files)
            elif isinstance(value, list):
                for item in value:
                    if isinstance(item, dict):
                        decode_nested_flatbuffer(item, temp_files)
                    elif isinstance(item, list):
                        # Handle nested lists
                        for subitem in item:
                            if isinstance(subitem, dict):
                                decode_nested_flatbuffer(subitem, temp_files)
    
    return data


def main(args):
    input_path = args[1]
    
    # Read the input JSON
    with open(input_path, 'r') as file:
        original_data = json.load(file)
    
    # Create a deep copy to avoid modifying the original
    processed_data = copy.deepcopy(original_data)
    
    # Track temp files for cleanup
    temp_files = []
    
    # Process nested FlatBuffers
    decode_nested_flatbuffer(processed_data, temp_files)
    
    # Clean up temp files
    for temp_file in temp_files:
        try:
            if os.path.exists(temp_file):
                os.remove(temp_file)
        except:
            pass
    
    # Write the output to a new file
    output_path = input_path.replace('.json', '_decoded.json')
    with open(output_path, 'w') as file:
        json.dump(processed_data, file, indent=2)
    
    print(f"\nProcessed JSON written to: {output_path}")
    print(f"Decoded {len(decoded_cache)} unique nested FlatBuffers")
                
if __name__ == '__main__':
    if len(sys.argv) >= 3:
        flatc_path = sys.argv[2]
    
    main(sys.argv)