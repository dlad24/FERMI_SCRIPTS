import sys
import glob
import serial
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import os
from datetime import datetime, timedelta


class live_plotter:
    def __init__(self,total_values,com_port):

        # initialize index values used for various readouts
        self.index = 0
        self.curr_values = 0
        self.fileno = 0

        #attempt serial connections
        try:
            self.ser = serial.Serial(com_port,115200,parity=serial.PARITY_NONE)
        except:
            print("Couldn't connect to pico")

        
        self.total_values = total_values
        self.start_time = datetime.now()

        #stores values for current, time, background, and hitrates
        self.current = np.zeros((3,total_values))
        self.timestamps = np.zeros(total_values)
        self.inv_timestamps = np.zeros(total_values)
        self.background = np.empty((3,1))
        self.spark_timestamps = [np.zeros(48,dtype=np.uint64) for i in range(3)]
        self.spark_active = [0,0,0]
        self.hour = 1
        self.hitrate_bins = np.arange(48)

        #root directory on QC-PC to store files
        self.root_directory = "C:/Users/QC-PC/Desktop/Current Monitor/"

        #default folder structure by date
        self.file_directory = self.root_directory+self.start_time.strftime("%Y_%m_%d/")
        if not os.path.exists(self.file_directory):
                        os.makedirs(self.file_directory)
    
        #define animated plots & average current
        self.fig,self.ax = plt.subplots(3,3,gridspec_kw={'width_ratios':[3,1,1]})
        plt.grid(visible=True,which='major',axis='x')
        self.lns = [[1 for j in range(3)] for i in range(3)]
        self.avg_currents = [1,1,1]

        #set data/styling for plots
        for i in range(3):
            (self.lns[i][0],) = self.ax[i][0].plot(self.timestamps,self.current[i],animated=True,color='r')
            (self.lns[i][1],) = self.ax[i][1].plot(self.timestamps[-150:],self.current[i][-150:],animated=True,color='r')
            (self.lns[i][2],) = self.ax[i][2].plot(self.hitrate_bins,self.spark_timestamps[i],animated=True,marker='o',linestyle = 'None',color='r')
            self.avg_currents[i] = self.ax[i][0].text(1.5,1.1, "", bbox={'facecolor':'w', 'alpha':0.5, 'pad':5},
                transform=self.ax[i][0].transAxes, ha="right")

        #plot titles
        self.ax[0][0].set_title('HV R&D table',loc='left',fontsize=24,color='b')
        self.ax[1][0].set_title('HV Conditioning table',loc='left',fontsize=24,color='b')
        self.ax[2][0].set_title('HV QC table',loc='left',fontsize=24,color='b')

        #set plots to fullscreen
        self.manager = plt.get_current_fig_manager()
        self.manager.full_screen_toggle()
        self.set_limits()

        #plot axis labels
        self.ax[2][0].set_xlabel('Time(s) - Last 5 Hours',fontsize=16)
        self.ax[2][1].set_xlabel('Time(s) - Last Minute',fontsize=16)
        self.ax[2][2].set_xlabel('Hitrate(hr) - Last 48 Hours',fontsize=16)

        #font size & additional styling
        for i in range(3):
            for j in range(3):
                self.ax[i][j].grid(visible=True,which='major',axis='x')
                for label in (self.ax[i][j].get_yticklabels()):
                    label.set_fontsize(20)
                if i == 2:
                    for label in (self.ax[i][j].get_xticklabels()):
                        label.set_fontsize(16)
                    continue
                else:
                    self.ax[i][j].tick_params(axis='x',which='both',bottom=False,top=False,labelbottom=False)   
        plt.show(block=False)
        plt.pause(0.1)

        #more animation settings
        self.bg = self.fig.canvas.copy_from_bbox(self.fig.bbox)

    # def load_old_data(self):
    #     """
    #     Checks for old data to load in current directory
    #     Helps with restarting code to not lose old measurements
    #     """
    #     try:
    #         row1 = np.loadtxt('hitrate_rd.csv',encoding='utf-8-sig',delimiter=',').T
    #         row2 = np.loadtxt('hitrate_cond.csv',encoding='utf-8-sig',delimiter=',').T
    #         row3 = np.loadtxt('hitrate_qc.csv',encoding='utf-8-sig',delimiter=',').T

    #         self.spark_timestamps = np.vstack((row1[0],row2[0],row3[0])

            

            

    def set_limits(self):
        #Set limits of the graph
        for i in range(3):
            self.ax[i][0].set_ylim(0,10)
            self.ax[i][0].set_xlim(0,18000)
            self.ax[i][1].set_ylim(0,10)
            self.ax[i][1].set_xlim(0,60)
            self.ax[i][0].set_ylabel('Current(uA)',fontsize=16)
            self.ax[i][2].set_ylim(0,1000)

            
    
    def grab_new_values(self):
        """
        Main function of readout

        Reads data from the serial port, then sorts it into various arrays for storage and plotting
        Should eventually be cut down into smaller functions
        """
        self.index+=1
        line = self.ser.readline()
        readout = 0

        if len(line) > 3:
            try:
                #decode and clean data from the pico, serial data prone to corrupted outputs so exits if failed
                readout = np.array(line.strip().decode('utf-8').split(),dtype=float).reshape(3,1)
                readout = readout*10 # scaling data for proper units
                self.background = np.hstack((self.background,readout))

                #writes out values after 50000 iterations
                self.curr_values +=1
                if self.curr_values > 50000:
                    self.write_out()

                #writes time of event and current value to arrays
                self.current = np.hstack((self.current,readout))
                event_time = datetime.now()
                delta_t = float((event_time-self.start_time).total_seconds())
                self.timestamps = np.append(self.timestamps,delta_t)

                #if values are too large cut them down
                self.check_overflow()

                """
                Spark detection algorithm

                If current value is above threshold (scaled background with offset) detect a spark
                """
                for i in range(3):
                    if (readout[i] > (np.average(self.background[i])*1.2+0.25) and self.background.shape[1] > 20 and self.spark_active[i] == 0):
                        self.spark_active[i] = 1 # flag prevents a single spark from being read out as multiple events

                        #hitrate determined by hour, move to next datapoint after one hour has passed
                        if (self.timestamps[-1]/(self.hour*3600)) > 1:
                            self.hour+=1

                            #hitrate_times = np.arange(np.arange(self.spark_timestamps[0].shape[1]))

                            #writeout hitrate to file
                            for j in range(3):
                                if j == 0:
                                    hit_file = self.file_directory+'hitrate_rd.csv'
                                if j == 1:
                                    hit_file = self.file_directory+'hitrate_cond.csv'
                                if j == 2:
                                    hit_file = self.file_directory+'hitrate_qc.csv'

                                
                                np.savetxt(hit_file,self.spark_timestamps[j],delimiter=',')
                                self.spark_timestamps[j] = np.insert(self.spark_timestamps[j],0,0)
                                self.spark_timestamps[j] = self.spark_timestamps[j][:-1]
                        #increment spark counter at location
                        self.spark_timestamps[i][0]+=1
                        if (i==2):
                            self.spark_detected(readout,event_time)
                    #reset flag once spark is back below threshold
                    if (readout[i] < np.average(self.background[i]*1.2)):
                        self.spark_active[i] = 0

                self.index = 0
                
            except ValueError:
                print('skipping bad value', readout)

    def spark_detected(self,readout,event_time):
        """
        Trigger for vidcapture system

        When sparks are detected, write out the timestamp to 'flag.txt' where the c code can log the event
        Flag file is used to communicate between the systems more easily than through other methods
        """
        flag_file = open('flag.txt','w')
        folder_delta = timedelta(hours=17) #makes the days start at 5pm instead of midnight
        offset_time = event_time-folder_delta
        self.file_directory = self.root_directory+offset_time.strftime("%Y_%m_%d/")
        if not os.path.exists(self.file_directory):
            os.makedirs(self.file_directory)
        vid_filename = event_time.strftime("%I_%M_%S_%p")
        flag_file.write(self.file_directory+vid_filename+'.avi')
        flag_file.close()
        print(readout[2],self.timestamps[-1])
        print('Spark detected... wrote to file: ', vid_filename)

    def update_plot(self):
        """
        Used to update the animated plots

        Each plot is filled with the updated array values, then redrawn on the display
        """
        self.fig.canvas.restore_region(self.bg)
        self.inv_timestamps = abs(self.timestamps[-1] - self.timestamps)
        
        for i in range(3):
            self.lns[i][0].set_ydata(self.current[i])
            self.lns[i][0].set_xdata(self.inv_timestamps)
            

            avg_current = float(np.average(self.current[i][-5000:]))
            self.avg_currents[i].set_text("Avg Current: {:.4f} uA".format(avg_current))

            self.ax[i][0].draw_artist(self.avg_currents[i])
            self.ax[i][0].draw_artist(self.lns[i][0])
            

            self.lns[i][1].set_ydata(self.current[i][-150:])
            self.lns[i][1].set_xdata(self.inv_timestamps[-150:])

            self.lns[i][2].set_ydata(self.spark_timestamps[i][:48])
            self.lns[i][2].set_xdata(self.hitrate_bins)

            self.ax[i][2].draw_artist(self.lns[i][2])
            
            
            self.ax[i][1].draw_artist(self.lns[i][1])

            # if self.timestamps[-1] > 60:
            #     self.ax[i][1].set_xlim(self.timestamps[-1]-60,self.timestamps[-1])

        #increases speed of readout
        self.fig.canvas.blit(self.fig.bbox)

        # self.fig.canvas.draw()
        self.fig.canvas.flush_events()
        # print(self.inv_timestamps[-150:])
        

            
    def write_out(self):
        #writes out current values to file
        date_time = datetime.now().strftime("%m-%d-%Y_%H-%M-%S_")
        write_file = self.file_directory+'current_out_'+date_time+str(self.fileno)+'.csv'
        print('wrote out current values to : ',write_file)
        np.savetxt(write_file,self.current,delimiter=',')
        self.fileno+=1
        self.curr_values =0

    def check_overflow(self):
        #limits max values of arrays to size of display
        if len(self.background) > 20:
            self.background = np.delete(self.background,0)
        if self.current.shape[1] > self.total_values:
            self.current = np.delete(self.current,0,1)
            self.timestamps = np.delete(self.timestamps,0)


if __name__ == '__main__':
    p1 = live_plotter(45000,'COM5')
    try:
        while True:
            p1.grab_new_values()
            # if p1.index % 100 ==0:
            p1.update_plot()
    except KeyboardInterrupt:
        p1.write_out()
        print('broke')
