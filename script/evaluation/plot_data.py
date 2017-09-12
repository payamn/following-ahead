#!/usr/bin/env python

import rospy
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import sys

# python script/evaluation/plot_data.py square 60 35 13 90
# python script/evaluation/plot_data.py eight 36 65 18 78

FONT_SIZE = 30
'''
data saved in the format 
    timestamp, range, robot_x, robot_y, person_x, person_y
    timestamp, bearing
'''
def showCorrespondingPoint(data, ax):
    font = {
        'family' : 'Times',
        'weight' : 'bold',
        'size'   : FONT_SIZE
    }
    axes = {
        'unicode_minus' : False
    }
    matplotlib.rc('font', **font)
    matplotlib.rc('axes', **axes)

    skip = 40
    for i in range(0, data.shape[0], skip):
        robot_x = data[i][2]
        robot_y = data[i][3]
        person_x = data[i][4]
        person_y = data[i][5]
        ax.plot(
            [robot_x, person_x], [robot_y, person_y], c='c', linewidth=2.5
        )
        
        markersize = 13
        markercolor_robot = 'r'
        markercolor_person = 'b'
        
        # if i == 0:
        #     markersize = 20
        #     markercolor_robot = (1.0, 0.5, 0)
        #     markercolor_person = (0, 0.5, 1.0)
        # elif (data.shape[0] - i) < skip:
        #     markersize = 20
        #     markercolor_robot = (1.0, 1.0, 0)
        #     markercolor_person = (0, 1.0, 1.0)

        if i == 0:
            ax.plot([robot_x], [robot_y], color=markercolor_robot, markersize=markersize, marker='o', label="robot")
            ax.plot([person_x], [person_y], color=markercolor_person, markersize=markersize, marker='*', label="person")
        else:
            ax.plot([robot_x], [robot_y], color=markercolor_robot, markersize=markersize, marker='o', )
            ax.plot([person_x], [person_y], color=markercolor_person, markersize=markersize, marker='*', )

    ax.legend(bbox_to_anchor=(1.2, 1.1))
    ax.set_xlim(-4, 4)
    ax.set_xlim(-4, 4)
    ax.set_aspect('equal', adjustable='box')#, 'datalim')
    # ax.set_xticks(np.arange(-3, 3, 0.5))
    # ax.set_yticks(np.arange(-3, 3, 0.5))
    ax.set_xlabel("x [m]")
    ax.set_ylabel("y [m]")

def showBearing(data, ax, start_time):
    font = {
        'family' : 'Times',
        'weight' : 'bold',
        'size'   : FONT_SIZE
    }
    axes = {
        'unicode_minus' : False
    }
    matplotlib.rc('font', **font)
    matplotlib.rc('axes', **axes)
    
    bearings = map(lambda x: x if x>-50 else -50, data[:, 1])
    ax.plot(data[:, 0]-start_time, bearings)
    ax.set_yticks(np.arange(-50, 100, 120))
    ax.set_xlabel("time [sec]")
    ax.set_ylabel("deg")
    
def showRange(data, ax, start_time):
    font = {
        'family' : 'Times',
        'weight' : 'bold',
        'size'   : FONT_SIZE
    }
    axes = {
        'unicode_minus' : False
    }
    matplotlib.rc('font', **font)
    matplotlib.rc('axes', **axes)
    
    ax.plot(data[:, 0]-start_time, data[:, 1])
    ax.set_yticks(np.arange(.2, 2.2, 1.8))
    # ax.set_xlabel("time [sec]")
    ax.set_ylabel("m")
    

if __name__ == '__main__':
    if len(sys.argv) < 6:
        print 'Usage: python plot_data.py <file-prefix> <traj_start> <traj_length> <plot_start> <plot_length>'
        exit(0)
    file_prefix= sys.argv[1]
    trajectory_start = int(sys.argv[2])
    trajectory_end = trajectory_start + int(sys.argv[3])
    plot_start = int(sys.argv[4])
    plot_end = plot_start + int(sys.argv[5])
    
    trajectory_data = np.loadtxt(file_prefix + "_trajectory_data.txt")
    bearing_data = np.loadtxt(file_prefix + "_bearing_data.txt")

    start_time = trajectory_data[0, 0]
    
    # -------------------- filter data -------------------- #
    trajectory_data_filtered = []
    bearing_data_filtered = []
    range_data_filtered = []
    for i in range(trajectory_data.shape[0]):
        if (trajectory_data[i, 0]  - start_time >= trajectory_start) and (trajectory_data[i, 0]  - start_time <= trajectory_end):
            trajectory_data_filtered.append(trajectory_data[i, :])
    
    for i in range(bearing_data.shape[0]):
        if (bearing_data[i, 0]  - start_time >= plot_start) and (bearing_data[i, 0]  - start_time <= plot_end):
            bearing_data_filtered.append(bearing_data[i, :])
    
    for i in range(trajectory_data.shape[0]):
        if (trajectory_data[i, 0]  - start_time >= plot_start) and (trajectory_data[i, 0]  - start_time <= plot_end):
            range_data_filtered.append(trajectory_data[i, :])
    
    trajectory_data = np.asarray(trajectory_data_filtered)
    bearing_data = np.asarray(bearing_data_filtered)
    range_data = np.asarray(range_data_filtered)
    print trajectory_data.shape
    print bearing_data.shape

    axes = []
    axes.append(plt.subplot2grid((21, 1), (0, 0), rowspan=15))
    axes.append(plt.subplot2grid((21, 1), (17, 0)))
    axes.append(plt.subplot2grid((21, 1), (19, 0), sharex=axes[1]))
    for i in range(len(axes)):
        axes[i].hold(True)
        axes[i].grid(True)
    showCorrespondingPoint(trajectory_data, axes[0])
    showRange(range_data, axes[1], range_data[0, 0])
    showBearing(bearing_data, axes[2], range_data[0, 0])
    plt.show()