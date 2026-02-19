#include "irifitnessfunction.h"

/******************************************************************************/
/******************************************************************************/

CIriFitnessFunction::CIriFitnessFunction(const char *pch_name,
										 CSimulator *pc_simulator,
										 unsigned int un_collisions_allowed_per_epuck)
	: CFitnessFunction(pch_name, pc_simulator)
{

	/* Check number of robots */
	m_pcSimulator = pc_simulator;
	TEpuckVector *pvecEpucks = m_pcSimulator->GetEpucks();

	if (pvecEpucks->size() == 0)
	{
		printf("No Robot, so fitness function can not be computed.\n Exiting...\n");
		fflush(stdout);
		exit(0);
	}
	else if (pvecEpucks->size() > 1)
	{
		printf("More than 1 robot, and fitness is not prepared for it.\n Exiting...\n");
	}

	m_pcEpuck = (*pvecEpucks)[0];

	m_unNumberOfSteps = 0;
	m_fComputedFitness = 0.0;

	m_unGreyFlag = 1;
	m_unGreyCounter = 0;
}

/******************************************************************************/
/******************************************************************************/

CIriFitnessFunction::~CIriFitnessFunction()
{
}
/******************************************************************************/
/******************************************************************************/

double CIriFitnessFunction::GetFitness()
{
	/* Total fitness calculated as the average fitness of each SimulationStep, and its penalized based on the collisions that have occurred and if the correct function is being executed */
	double fit = (m_fComputedFitness / (double)m_unNumberOfSteps) * ((double)(fmin(m_unGreyCounter, 20.0) / 20.0));
	/* A change of objective is encouraged through the application of this penalty */
	if (m_unGreyFlag == 1)
		fit /= 10.0;

	/* 	If fitness less than 0, put it to 0
		If fitness greater than 1, put it to 1 */
	if (fit < 0.0) fit = 0.0;
	if (fit > 1.0) fit = 1.0;

	return fit;
}

/******************************************************************************/
/******************************************************************************/
void CIriFitnessFunction::SimulationStep(unsigned int n_simulation_step, double f_time, double f_step_interval)
{
	/* Get actual SPEED of the left and right wheel */
	double leftSpeed = 0.0;
	double rightSpeed = 0.0;
	m_pcEpuck->GetWheelSpeed(&leftSpeed, &rightSpeed);
	leftSpeed = 0.5 + (leftSpeed / (2.0 * m_pcEpuck->GetMaxWheelSpeed()));
	rightSpeed = 0.5 + (rightSpeed / (2.0 * m_pcEpuck->GetMaxWheelSpeed()));

	/* Eval maximum speed partial fitness */
	double maxSpeedEval = (fabs(leftSpeed - 0.5) + fabs(rightSpeed - 0.5));

	/* Eval same direction partial fitness */
	double sameDirectionEval = 1 - sqrt(fabs(leftSpeed - rightSpeed));

	/* Eval SENSORS */
	/* Where the Max PROXIMITY sensor will be stored*/
	double maxProxSensor = 0.0;
	/* Where the Max RED LIGHT sensor will be stored*/
	double maxRedLightSensor = 0.0;
	/* Where the Max BLUE LIGHT sensor will be stored*/
	double maxBlueLightSensor = 0.0;

	/* Where the Mean RED LIGHT sensor will be stored */
	double meanRedLightSensors = 0.0;
	/* Where the Mean BLUE LIGHT sensor will be stored */
	double meanBlueLightSensors = 0.0;

	/* Where the GROUND MEMORY sensor will be stored */
	double *groundMemory;

	/* Auxiluar variables */
	unsigned int unThisSensorsNumberOfInputs;
	double *pfThisSensorInputs;

	/* Go in all the sensors */
	TSensorVector vecSensors = m_pcEpuck->GetSensors();
	for (TSensorIterator i = vecSensors.begin(); i != vecSensors.end(); i++)
	{
		/* Check type of sensor */
		switch ((*i)->GetType())
		{
		/* If sensor is PROXIMITY */
		case SENSOR_PROXIMITY:
			/* Get the number of inputs */
			unThisSensorsNumberOfInputs = (*i)->GetNumberOfInputs();
			/* Get the actual values */
			pfThisSensorInputs = (*i)->GetComputedSensorReadings();

			/* For every input */
			for (int j = 0; j < unThisSensorsNumberOfInputs; j++)
			{
				/* If reading bigger than maximum */
				if (pfThisSensorInputs[j] > maxProxSensor)
				{
					/* Store maximum value */
					maxProxSensor = pfThisSensorInputs[j];
				}
			}
			break;

		/* If sensor is GROUND_MEMORY */
		case SENSOR_GROUND_MEMORY:
			/* Get the actual value */
			groundMemory = (*i)->GetComputedSensorReadings();
			break;
		
		/* If sensor is RED LIGHT */
		case SENSOR_REAL_RED_LIGHT:
			unThisSensorsNumberOfInputs = (*i)->GetNumberOfInputs();
			/* Get the actual values */
			pfThisSensorInputs = (*i)->GetComputedSensorReadings();

			for (int j = 0; j < unThisSensorsNumberOfInputs; j++)
			{
				/* Store the sensors value */
				meanRedLightSensors += pfThisSensorInputs[j];

				/* If reading bigger than maximum */
				if (pfThisSensorInputs[j] > maxRedLightSensor)
				{
					/* Store maximum value */
					maxRedLightSensor = pfThisSensorInputs[j];
				}
			}
			meanRedLightSensors /= unThisSensorsNumberOfInputs;
			break;

		/* If sensor is BLUE LIGHT */
		case SENSOR_REAL_BLUE_LIGHT:
			unThisSensorsNumberOfInputs = (*i)->GetNumberOfInputs();
			/* Get the actual values */
			pfThisSensorInputs = (*i)->GetComputedSensorReadings();

			for (int j = 0; j < unThisSensorsNumberOfInputs; j++)
			{
				/* Store the sensors value */
				meanBlueLightSensors += pfThisSensorInputs[j];

				/* If reading bigger than maximum */
				if (pfThisSensorInputs[j] > maxBlueLightSensor)
				{
					/* Store maximum value */
					maxBlueLightSensor = pfThisSensorInputs[j];
				}
			}
			meanBlueLightSensors /= unThisSensorsNumberOfInputs;
			break;
		}
	}

	maxProxSensor = 1 - maxProxSensor;

	double fitness = 1.0;

	/* Evaluate if the robot moves quickly, in the same direction, and avoids obstacles */
	fitness *= maxSpeedEval * sameDirectionEval * maxProxSensor * (leftSpeed * rightSpeed);

	/* Evaluate the movement toward one light or another depending on GroundMemory sensor, and pe */
	if (groundMemory[0] > 0.0)
	{
		/* Penalize if it approaches the red light beyond a certain threshold */
		if(maxRedLightSensor > 0.93)
			fitness -= (maxRedLightSensor - 0.5);

		fitness *= meanBlueLightSensors;
		if (m_unGreyFlag == 0)
		{
			m_unGreyFlag = 1;
			m_unGreyCounter++;
		}
	}
	else
	{
		fitness *= meanRedLightSensors;
		if (m_unGreyFlag == 1)
		{
			m_unGreyFlag = 0;
			m_unGreyCounter++;
		}
	}

	m_unNumberOfSteps++;
	m_fComputedFitness += fitness;
}

/******************************************************************************/
/******************************************************************************/
