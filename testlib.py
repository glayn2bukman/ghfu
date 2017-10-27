from ctypes import *
ghfu = CDLL("./libghfu.so")

ghfu.init()

ghfu.register_member(None, "brocker 1", c_float(10+40)) # ID=1, if account is registered successully

ghfu.register_member(ghfu.get_account_by_id(1), "investor 1", c_float(10+40+180+958))

ghfu.invest_money(ghfu.get_account_by_id(1), c_float(40+10+180+1450), "First Package",2,1); # ID=2

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
    ghfu.auto_refill(None, monthly_percentage)

ghfu.structure_details(None)

print "\nsystem float =$%.2f, total commissions = $%.2f"%( c_float.in_dll(ghfu, "SYSTEM_FLOAT").value,
      c_float.in_dll(ghfu, "CUMULATIVE_COMMISSIONS").value)
