# the most important interfaces to the dynamic library (libjermGHFU.so) are;
#   ID account_id(Account account);
#   bool dump_structure_details(ID account_id, String fout_name); 
#   Account get_account_by_id(const ID id);
#   ID account_id(Account account);
#   bool invest(ID account_id, const Amount amount, const String package, 
#        const ID package_id, const bool update_system_float, String fout_name);
#   void perform_monthly_operations(float auto_refill_percentages[4][2], String fout_name);
#   void purchase_property(ID IB_account_id, const Amount amount, const bool member, 
#        const String buyer_names, String fout_name);
#   bool redeem_account_points(ID account_id, Amount amount, String fout_path);
#   ID register_new_member(ID uplink_id, String names, Amount amount, String fout_name);

#   NB: String is defined as (char *) in ghfu.c. bool is defined as (enum bool {false, true}),
#       ID is defined as (unsigned long)

import sys, json
from ctypes import *
ghfu = CDLL("lib/libjermGHFU.so")
jermCrypt = CDLL("lib/libjermCrypt.so")
#jermCrypt.encrypt_file(fin,pswd,fout,int overwrite,int verbose)
#jermCrypt.decrypt_file(fin,pswd,fout,int overwrite,int verbose)

ghfu.init("files/out/test-init")

with open("files/out/test-init","r") as f: 
    error = f.read()
    if error:sys.exit(error)

id1 = ghfu.register_new_member(None, "brocker 1", c_float(10+40), "files/out/test-b1") # ID=1, if account is registered successully

id2 = ghfu.register_new_member(id1, "investor 1", c_float(10+40+180+958), "files/out/test-i1")

ghfu.invest(id1, c_float(40+10+180+1450), "First Package",2,1); # ID=2

# create monthly %s for auto-refill...
m_a_r_p = ((c_float*2)*4)*12 # 12 arrays, each with 4 items each in turn with 2-items

monhtly_auto_refill_percentages = m_a_r_p(
    # all arrays MUST be in descending rank order and MUST end with the (0,0) terminating condition */
    ( (375,64),(250,60),(125,49),(0,0) ),        

    ( (375,60),(250,65), (125,60),(0,0) ),        

    ( (375,43), (250,51), (125,54), (0, 0) ),        

    ( (375,60), (250,65), (125,60), (0,0) ),     

    ( (375,78), (250,69), (125,65), (0,0) ),        

    ( (375,67), (250,63), (125,59), (0, 0) ),        

    ( (375,80), (250,69), (125,65), (0,0) ),        

    ( (375,58), (250,42), (125,48), (0,0) ),        

    ( (375,53), (250,65), (125,60), (0,0) ),        

    ( (375,49), (250,65), (125,52), (0,0) ),        

    ( (375,67), (250,58), (125,49), (0,0) ),        

    ( (375,62), (250,66), (125,48), (0,0) ),
)

for monthly_percentage in monhtly_auto_refill_percentages:
    ghfu.monthly_operations(monthly_percentage)

if ghfu.dump_structure_details(id1, "files/json/1.json"):
    print "account data:",json.JSONDecoder().decode(open("files/json/1.json","r").read())
else: print "failed to dump account details to file!"

print "\nsystem float =$%.2f, total commissions = $%.2f"%( c_float.in_dll(ghfu, "SYSTEM_FLOAT").value,
      c_float.in_dll(ghfu, "CUMULATIVE_COMMISSIONS").value)

c_char_p.in_dll(ghfu,"ACCOUNTS").value = "hello, world"
print c_char_p.in_dll(ghfu,"ACCOUNTS").value[:]
