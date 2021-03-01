#!/usr/bin/python3

#  Copyright (C) 2021 LEIDOS.
# 
#  Licensed under the Apache License, Version 2.0 (the "License"); you may not
#  use this file except in compliance with the License. You may obtain a copy of
#  the License at
# 
#  http://www.apache.org/licenses/LICENSE-2.0
# 
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#  License for the specific language governing permissions and limitations under
#  the License.

import sys
from bisect import bisect_left 
import math

def binarySearch(a, x): 
    return bisect_left(a, x) 
    

def filter_curvatures(curvatures, downtrack_step_size):

  #brackets = compute_curvature_brackets(2.5, 2.2352, 35.7632)
  #print("Brackets: " + str(brackets))
  #c1 = constrain_to_brackets(brackets, curvatures, 1)
  #c2 = denoise(c1, 4)
  c3 = moving_average_filter(curvatures, 8)
  return c3
  #return apply_curvature_rate_limits(c2, downtrack_step_size, 0.039)

def moving_average_filter(input, window_size, ignore_first_point=False):

  output = []

  if len(input) == 0:
    return output

  start_index = 0
  if ignore_first_point:
    start_index = 1
    output.append(input[0])

  for i in range(start_index, len(input)):    
    
    total = 0
    sample_min = int(max(0, i - int(window_size) / int(2)))
    sample_max = int(min( len(input) - 1 , i + int(window_size) / int(2)))

    count = sample_max - sample_min + 1
    sample = []
    for j in range(sample_min, sample_max + 1):
      total += input[j]
    
    output.append(total / float(count))


  return output

def apply_curvature_rate_limits(curvatures, downtrack_step_size, max_curvature_rate):
  output = []
  if len(curvatures) == 0:
    return output

  output.append(curvatures[0])
  
  for i in range(1, len(curvatures)):
    delta_d = downtrack_step_size
    prev_curvature = output[-1]
    cur_curvature = curvatures[i]
    new_curvature = cur_curvature

    if cur_curvature > prev_curvature:
      # // Acceleration case
      limited_curvature = prev_curvature + (max_curvature_rate * delta_d)
      new_curvature = min(cur_curvature, limited_curvature)
    
    elif cur_curvature < prev_curvature:
      #  // Deceleration case
      limited_curvature = prev_curvature - (max_curvature_rate * delta_d)
      new_curvature = max(cur_curvature, limited_curvature)

    new_curvature = max(0.0, new_curvature)
    output.append(new_curvature)
    
  return output

def compute_curvature_brackets(acceleration_limit, bracket_size, max_value):

  curvature_bracket_upper_bounds = [sys.float_info.max]
  velocity = bracket_size # 2.2352 5mph as m/s

  while velocity < max_value:  # 35.7632 Less than 80mph
    curvature_bracket_upper_bounds.append(acceleration_limit / (velocity*velocity)) # curvature = a / v^2
    velocity += bracket_size
  
  curvature_bracket_upper_bounds.reverse()
  return curvature_bracket_upper_bounds

def constrain_to_brackets(brackets, values, round_direction=1):# -1 is down, 0 is rounding, 1 is round up
  output = []

  for val in values:
    index = binarySearch(brackets, val)
    if index == 0:
      output.append(brackets[0])
    elif index == len(brackets) - 1:
      output.append(brackets[-1])
    else:
      low_val = brackets[index - 1]
      high_val = brackets[index]
      if round_direction == -1:
        output.append(low_val)
      elif round_direction == 0: 
        halfway = ((high_val - low_val) / 2.0) + low_val
        if val < halfway:
            output.append(low_val); # Round down
        else:
            output.append(high_val); # Round up
      else:
        output.append(high_val)


  return output

def denoise(values, required_size):

  if len(values) == 0:
    print("Empty values")
    return []

  sections = []
  sections.append(
    [0, values[0], 1] # start index, value, element count
  )

  i = 0
  for v in values:
    if i == 0:
      i+=1
      continue

    prev_section_value = sections[-1][1]
    if v == prev_section_value:
      sections[-1][2] += 1
      continue
    
    sections.append(
      [i, v, 1] # start index, value, element count
    )
    i += 1

  if len(sections) == 1:
    return values # All values were the same

  #print("TimeStep: ")
  #print("Sections: " + str(sections))
  
  # TODO This loop doesn't really make much sense. It can cause your curvature to be pulled down while in a small turn and can 
  # TODO this loop might have a risk of pulling up the whole dataset
  # Do we want to give preference based on size? That runs the risk of pulling down the curve
  keep_looping = True
  while keep_looping:
    keep_looping = False
    i = 0
    for section in sections:
      if section[2] < required_size:
        ref_sec = section
        if i == 0:
          ref_sec = sections[i+1]
        elif i == len(sections) - 1:
          ref_sec = sections[i-1]
        elif sections[i+1][1] >= sections[i-1][1]:
          ref_sec = sections[i+1]
        else:
          ref_sec = sections[i-1]
        
        #print("Index: " + str(i))
        #print("RefSecSize1: " + str(ref_sec[2]))
        ref_sec[2] += section[2] # Update the absorbing section count
        #print("RefSecSize2: " + str(ref_sec[2])) # TODO This update process is causing a flop back and forth so this never exits
        del sections[i] # This is an attempt to delete during iteration. Must use index behavior here to get required functionality

        if ref_sec[2] < required_size:
          keep_looping = True
          break
      i+=1
  
  #print("After mod Sections: " + str(sections))
  output = []
  for section in sections:
    j = 0
    while j < section[2]:
      output.append(section[1])
      j += 1
#  print("Len Values: " + str(len(values)) + " Len Output: " + str(len(output)))
  return output



def local_curvatures(centerline_points):


  if len(centerline_points) == 0:
    return []

  spatial_derivative = compute_finite_differences_1(centerline_points)
  normalized_tangent_vectors = normalize_vectors(spatial_derivative)

  arc_lengths = compute_arc_lengths(centerline_points)

  tangent_derivative =  compute_finite_differences_2(
      normalized_tangent_vectors, 
      arc_lengths)

  curvature = compute_magnitude_of_vectors(tangent_derivative)

  return curvature

def compute_magnitude_of_vectors(data):
  out = []
  for d in data:
    out.append(math.sqrt(d[0]*d[0] + d[1]*d[1]))
  return out

def compute_finite_differences_1(data):

  out = []
  diff = [0,0]; # Two element vec
  vec_i_plus_1 = []; # Two element vec
  vec_i_minus_1 = []; # Two element vec

  i = 0
  for d in data:
    if i == 0:
      # Compute forward derivative
      diff[0] = (data[i + 1][0] - data[i][0])
      diff[1] = (data[i + 1][1] - data[i][1])
    elif i == len(data) - 1:
      # Compute backward derivative
      diff[0] = (data[i][0] - data[i - 1][0])
      diff[1] = (data[i][1] - data[i - 1][1])
    else:
      # Else, compute centered derivative
      vec_i_plus_1 = data[i + 1]
      vec_i_minus_1 = data[i - 1]
      diff[0] = (vec_i_plus_1[0] - vec_i_minus_1[0])/2.0
      diff[1] = (vec_i_plus_1[1] - vec_i_minus_1[1])/2.0
    

    out.append(diff)
    i+=1

  return out


def compute_finite_differences_2(x,y):
  out = []
  diff = [0,0] # two element vec
  i = 0
  for v in x:
    if i == 0:
      dx = x[i + 1][0] - x[i][0]
      dy = x[i + 1][1] - x[i][1]
      dd = y[i + 1] - y[i]
      diff[0] = dx/dd
      diff[1] = dy/dd
    elif i == len(x) - 1:
      dx = x[i][0] - x[i - 1][0]
      dy = x[i][1] - x[i - 1][1]
      dd = y[i] - y[i - 1]
      diff[0] = dx/dd
      diff[1] = dy/dd
    else:
      dx = x[i + 1][0] - x[i - 1][0]
      dy = x[i + 1][1] - x[i - 1][1]
      dd = y[i + 1] - y[i - 1]
      diff[0] = dx/dd
      diff[1] = dy/dd
    
    out.append(diff)
    i += 1

  return out

def normalize_vectors(vectors):
  out = []
  for vec in vectors:
    mag = math.sqrt(vec[0]*vec[0] + vec[1]*vec[1])
    vec[0] = vec[0] / mag
    vec[1] = vec[1] / mag
    out.append(vec)

  return out


def compute_arc_lengths(data):
  out = []
  total = 0
  diff = 0
  i = 0
  for d in data: 
    if i == 0:
      out.append(0)
    else:
      dx = data[i][0] - data[i - 1][0]
      dy = data[i][1] - data[i - 1][1]
      diff = math.sqrt(dx*dx + dy*dy)
      total += diff
      out.append(total)

    i += 1
  

  return out