import cv2
import numpy as np
import os

# placeholder to grab first image 
final = 0

# Loop through files in directory
try:
    for file in os.listdir():
        if (str(file)[-3:] != 'bmp'): # Only look at BMP files
            continue
        if (str(file)) == 'baseline.jpg': # Ignore baseline
            continue
        if (final ==0 ):
            final = cv2.imread(str(file),cv2.COLOR_BGR2GRAY)
        image = cv2.imread(str(file),cv2.COLOR_BGR2GRAY) # Read image as grayscale
        #cv2.imshow('image',final) # Show image for Debug
        #cv2.waitKey(0)
        if cv2.countNonZero(image) < 500: # Extra check to throw out garbage images/prevent one bad image from overlaying entire heatmap
            final = np.add(image,final) # Overlay white pixels to baseline image
            print(str(file)) # debug
except KeyboardInterrupt: # Ctrl-c to cancel
    print('broke')

# Optional code block to color the active pixels
final = cv2.cvtColor(final,cv2.COLOR_GRAY2BGR)
ret, thresh = cv2.threshold(final,100,255,cv2.THRESH_BINARY)
cv2.imshow('thresh',thresh)
##final[final==[255,255,255]] = [0,0,255]

##thresh[np.where((thresh==[255, 255, 255]).all(axis=2))] = [0, 0, 255]
cv2.imshow('final',thresh)
cv2.waitKey(0)

print(final)

#Make sure images are same size (debug)
baseline = cv2.imread('baseline.jpg',cv2.COLOR_GRAY2BGR)
print(final.shape)
print(baseline.shape)


# Overlay sparking regions onto baseline image with desired weights
overlay = cv2.addWeighted(baseline,0.4,thresh,0.4,0)
cv2.imshow('mask',overlay)
cv2.waitKey(0)

# save resulting masked image
cv2.imwrite('overlay.jpg',overlay)
