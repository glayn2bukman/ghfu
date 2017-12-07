# in case this program raises a "port used error" for servers, use
#   $ netstat -tulpn | grep $(PORT=NNNNN; echo $PORT)
# to get the PID of the previous server running at that port and then kill it with
#   $ kill -9 PID
# after this, you can re-run the driver program

program_path="$(pwd)"
python "$program_path/Server/Jpesa/server.py"&
python "$program_path/Server/server.py"&
python "$program_path/Server/perform_monthly_operations.py"&
