public int continue1(int z)
{
	while (z != 0)
	{
		z = z - 1;
		if (z > 10)
			continue;
		z = z + 1;
	}
	
	return 7;
}

public int break1(int z)
{
	while (z != 0)
	{
		z = z - 1;
		if (z > 10)
			break;
		z = z + 1;
	}
	
	return 7;
}
