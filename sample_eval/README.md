Installation:
============

To install, go to **install** folder and launch : 

	conda create --name zerospeech --file requirements.txt.

Then : 

	pip install -r pip-requirements.txt.
	
If all went well go to src/ABXpy and do : 

	make install && make test.

Example:
=======

To do the evaluation, do : 

	./eval1 /path/to/your/feature/folder/ length_of_the_files corpus /path/to/your/output/folder/ .

For example, for features extracted on 1s files for the chinese corpus : 

	./eval1 /home/julien/data 1 chinese temp/


