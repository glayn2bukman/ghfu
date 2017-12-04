program_path="$(pwd)"
python "$program_path/Server/Jpesa/server.py"&
python "$program_path/Server/server.py"&
python "$program_path/Server/perform_monthly_operations.py"&
