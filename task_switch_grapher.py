import matplotlib.pyplot as plt

in_file = open('tasks.log', 'r')
thisfile = in_file.readlines()

app_list = list()
ui_list = list()
act_list = list()
hrm_list = list()

last_time = 0

for line in thisfile:
    if line == thisfile[len(thisfile)-1]: #skip last line in case it wasn't completely written to file
        break
    #print(line)
    temp = line.split(":")
    task = temp[0].strip()
    timestamp = int(temp[1].strip()) #maybe make this float if we can get better timestamp
    
    new_period = (last_time, timestamp - last_time)
    
    if last_time > timestamp:
        break #we messed up

    last_time = timestamp


    if task == "app":
        app_list.append(new_period)
    elif task == "activity":
        act_list.append(new_period)
    elif task == "ui":
        ui_list.append(new_period)
    elif task == "hrm":
        hrm_list.append(new_period)


fig, ax = plt.subplots()
ax.broken_barh(app_list, (10, 9), facecolors='tab:blue')
ax.broken_barh(ui_list, (20, 9), facecolors='tab:orange')
ax.broken_barh(act_list, (30, 9), facecolors='tab:green')
ax.broken_barh(hrm_list, (40, 9), facecolors='tab:red')
ax.set_ylim(5, 55)
# ax.set_xlim(0, 200)
ax.set_xlabel('ticks since start')
ax.set_yticks([15, 25, 35, 45])
ax.set_yticklabels(['App', 'UI', 'Activity', 'HRM'])
ax.grid(True)

plt.show()S
